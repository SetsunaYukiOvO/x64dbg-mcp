/**
 * @file MCPHttpServer.cpp
 * @brief MCP HTTP Server implementation
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "MCPHttpServer.h"
#include "../core/ConfigManager.h"
#include "../core/Logger.h"
#include "../core/MethodDispatcher.h"
#include "../core/JSONRPCParser.h"
#include "../core/MCPToolRegistry.h"
#include "../core/MCPResourceRegistry.h"
#include "../core/MCPPromptRegistry.h"
#include <ws2tcpip.h>
#include <sstream>
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <limits>
#include <nlohmann/json.hpp>

#pragma comment(lib, "ws2_32.lib")

namespace MCP {

namespace {

constexpr size_t kReceiveChunkSize = 4096;
constexpr size_t kMaxHttpRequestSize = 1024 * 1024;
constexpr int kMaxRequestsPerSecond = 100;

struct RateLimiter {
    std::mutex mutex;
    int count = 0;
    std::chrono::steady_clock::time_point windowStart = std::chrono::steady_clock::now();

    bool Allow() {
        std::lock_guard<std::mutex> lock(mutex);
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - windowStart).count();
        if (elapsed >= 1000) {
            count = 0;
            windowStart = now;
        }
        if (++count > kMaxRequestsPerSecond) {
            return false;
        }
        return true;
    }
};

RateLimiter g_rateLimiter;

std::string ToLowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string GetHttpHeader(const std::string& request, const std::string& headerName) {
    const size_t headerEnd = request.find("\r\n\r\n");
    const std::string headers = (headerEnd != std::string::npos) ? request.substr(0, headerEnd) : request;

    const std::string lowerHeaders = ToLowerCopy(headers);
    const std::string search = ToLowerCopy(headerName) + ":";
    const size_t pos = lowerHeaders.find(search);
    if (pos == std::string::npos) {
        return "";
    }

    size_t valueStart = pos + search.length();
    while (valueStart < headers.size() && (headers[valueStart] == ' ' || headers[valueStart] == '\t')) {
        valueStart++;
    }
    const size_t valueEnd = headers.find('\r', valueStart);
    if (valueEnd == std::string::npos) {
        return "";
    }

    return headers.substr(valueStart, valueEnd - valueStart);
}

// Parse a requestId that was produced by json::dump(), falling back to null on failure.
nlohmann::json SafeParseId(const std::string& requestId) {
    if (requestId == "null") return nullptr;
    try {
        return nlohmann::json::parse(requestId);
    } catch (const nlohmann::json::exception&) {
        return nullptr;
    }
}

void TrimInPlace(std::string& value) {
    const auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };
    value.erase(value.begin(),
                std::find_if(value.begin(), value.end(),
                             [&](unsigned char ch) { return !isSpace(ch); }));
    value.erase(std::find_if(value.rbegin(), value.rend(),
                             [&](unsigned char ch) { return !isSpace(ch); }).base(),
                value.end());
}

bool ParseContentLength(const std::string& headers, size_t& outLength) {
    std::istringstream stream(headers);
    std::string line;

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        const size_t colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }

        std::string name = line.substr(0, colon);
        TrimInPlace(name);
        if (ToLowerCopy(name) != "content-length") {
            continue;
        }

        std::string value = line.substr(colon + 1);
        TrimInPlace(value);
        if (value.empty()) {
            return false;
        }

        try {
            const size_t parsed = static_cast<size_t>(std::stoull(value));
            outLength = parsed;
            return true;
        } catch (...) {
            return false;
        }
    }

    return false;
}

bool ReceiveHttpRequest(SOCKET socket, std::string& request, bool& payloadTooLarge) {
    request.clear();
    payloadTooLarge = false;

    std::array<char, kReceiveChunkSize> buffer{};
    size_t headerEnd = std::string::npos;
    size_t contentLength = 0;
    bool lengthKnown = false;

    while (true) {
        int bytesReceived = recv(socket, buffer.data(), static_cast<int>(buffer.size()), 0);
        if (bytesReceived <= 0) {
            break;
        }

        request.append(buffer.data(), static_cast<size_t>(bytesReceived));
        if (request.size() > kMaxHttpRequestSize) {
            payloadTooLarge = true;
            return false;
        }

        if (headerEnd == std::string::npos) {
            headerEnd = request.find("\r\n\r\n");
            if (headerEnd != std::string::npos) {
                const std::string headerPart = request.substr(0, headerEnd + 4);
                if (!ParseContentLength(headerPart, contentLength)) {
                    contentLength = 0;
                }
                if (contentLength > kMaxHttpRequestSize) {
                    payloadTooLarge = true;
                    return false;
                }
                lengthKnown = true;
            }
        }

        if (headerEnd != std::string::npos && lengthKnown) {
            const size_t totalExpected = headerEnd + 4 + contentLength;
            if (request.size() >= totalExpected) {
                if (request.size() > totalExpected) {
                    request.resize(totalExpected);
                }
                return true;
            }
        }
    }

    if (headerEnd != std::string::npos && lengthKnown) {
        const size_t totalExpected = headerEnd + 4 + contentLength;
        if (request.size() >= totalExpected) {
            if (request.size() > totalExpected) {
                request.resize(totalExpected);
            }
            return true;
        }
    }

    return false;
}

bool ResolveHostAddress(const std::string& host, in_addr& address) {
    if (host == "0.0.0.0" || host == "*") {
        address.s_addr = htonl(INADDR_ANY);
        return true;
    }

    if (inet_pton(AF_INET, host.c_str(), &address) == 1) {
        return true;
    }

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* result = nullptr;
    const int resolveResult = getaddrinfo(host.c_str(), nullptr, &hints, &result);
    if (resolveResult != 0 || result == nullptr) {
        return false;
    }

    const auto* resolved = reinterpret_cast<sockaddr_in*>(result->ai_addr);
    address = resolved->sin_addr;
    freeaddrinfo(result);
    return true;
}

bool SendAll(SOCKET socket, const std::string& data) {
    size_t totalSent = 0;
    while (totalSent < data.size()) {
        const size_t remaining = data.size() - totalSent;
        const int chunkSize = remaining > static_cast<size_t>(std::numeric_limits<int>::max())
            ? std::numeric_limits<int>::max()
            : static_cast<int>(remaining);
        const int sent = send(
            socket,
            data.data() + totalSent,
            chunkSize,
            0
        );

        if (sent == SOCKET_ERROR || sent == 0) {
            return false;
        }

        totalSent += static_cast<size_t>(sent);
    }

    return true;
}

const char* GetHttpStatusText(int statusCode) {
    switch (statusCode) {
        case 200: return "OK";
        case 202: return "Accepted";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Payload Too Large";
        case 500: return "Internal Server Error";
        default:  return "OK";
    }
}

} // namespace

std::mutex MCPHttpServer::s_activeInstanceMutex;
MCPHttpServer* MCPHttpServer::s_activeInstance = nullptr;

MCPHttpServer::MCPHttpServer() 
    : m_listenSocket(INVALID_SOCKET)
    , m_running(false)
    , m_requestId(0)
{
}

MCPHttpServer::~MCPHttpServer() {
    Stop();
}

bool MCPHttpServer::BroadcastNotification(const std::string& method, const nlohmann::json& params) {
    MCPHttpServer* active = nullptr;
    {
        std::lock_guard<std::mutex> lock(s_activeInstanceMutex);
        active = s_activeInstance;
    }

    if (!active || !active->IsRunning()) {
        return false;
    }

    nlohmann::json notification = {
        {"jsonrpc", "2.0"},
        {"method", method},
        {"params", params}
    };

    return active->BroadcastSSEEvent("message", notification.dump());
}

bool MCPHttpServer::Start(const std::string& host, int port) {
    if (m_running) {
        Logger::Error("HTTP Server already running");
        return false;
    }

    if (port <= 0 || port > 65535) {
        Logger::Error("Invalid port: {}", port);
        return false;
    }

    m_host = host;
    m_port = port;

    // 鍒濆鍖?WinSock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        Logger::Error("WSAStartup failed");
        return false;
    }

    // 鍒涘缓鐩戝惉 socket
    m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listenSocket == INVALID_SOCKET) {
        Logger::Error("Failed to create socket");
        WSACleanup();
        return false;
    }

    int reuseAddr = 1;
    setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuseAddr), sizeof(reuseAddr));

    // 缁戝畾鍦板潃
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(static_cast<u_short>(port));
    if (!ResolveHostAddress(host, serverAddr.sin_addr)) {
        Logger::Error("Invalid listen host: {}", host);
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
        WSACleanup();
        return false;
    }

    if (bind(m_listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        Logger::Error("Failed to bind socket: {}", WSAGetLastError());
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
        WSACleanup();
        return false;
    }

    // 寮€濮嬬洃鍚?
    if (listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        Logger::Error("Failed to listen: {}", WSAGetLastError());
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
        WSACleanup();
        return false;
    }

    m_running = true;

    {
        std::lock_guard<std::mutex> lock(s_activeInstanceMutex);
        s_activeInstance = this;
    }

    m_serverThread = std::thread(&MCPHttpServer::ServerLoop, this);

    Logger::Info("MCP HTTP Server started on http://" + m_host + ":" + std::to_string(m_port));
    return true;
}

void MCPHttpServer::Stop() {
    bool hasTasks = false;
    {
        std::lock_guard<std::mutex> lock(m_clientTasksMutex);
        hasTasks = !m_clientTasks.empty();
    }
    bool hasActiveClients = false;
    {
        std::lock_guard<std::mutex> lock(m_activeClientSocketsMutex);
        hasActiveClients = !m_activeClientSockets.empty();
    }
    if (!m_running && m_listenSocket == INVALID_SOCKET && !m_serverThread.joinable() &&
        !hasTasks && !hasActiveClients) {
        return;
    }

    m_running = false;

    if (m_listenSocket != INVALID_SOCKET) {
        shutdown(m_listenSocket, SD_BOTH);
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
    }

    {
        std::lock_guard<std::mutex> lock(m_activeClientSocketsMutex);
        for (SOCKET clientSocket : m_activeClientSockets) {
            shutdown(clientSocket, SD_BOTH);
        }
    }

    if (m_serverThread.joinable()) {
        m_serverThread.join();
    }

    std::vector<std::future<void>> clientTasks;
    {
        std::lock_guard<std::mutex> lock(m_clientTasksMutex);
        clientTasks.swap(m_clientTasks);
    }
    for (auto& task : clientTasks) {
        if (!task.valid()) {
            continue;
        }
        try {
            task.get();
        } catch (const std::exception& ex) {
            Logger::Error("Client task ended with exception: {}", ex.what());
        } catch (...) {
            Logger::Error("Client task ended with unknown exception");
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_activeClientSocketsMutex);
        m_activeClientSockets.clear();
    }
    {
        std::lock_guard<std::mutex> lock(m_sseClientSocketsMutex);
        m_sseClientSockets.clear();
    }
    {
        std::lock_guard<std::mutex> lock(s_activeInstanceMutex);
        if (s_activeInstance == this) {
            s_activeInstance = nullptr;
        }
    }

    WSACleanup();
    Logger::Info("MCP HTTP Server stopped");
}

void MCPHttpServer::ServerLoop() {
    while (m_running) {
        CleanupFinishedClientTasks();

        SOCKET clientSocket = accept(m_listenSocket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) {
            if (m_running) {
                Logger::Error("Accept failed");
                m_running = false;
            }
            break;
        }

        {
            std::lock_guard<std::mutex> lock(m_activeClientSocketsMutex);
            m_activeClientSockets.insert(clientSocket);
        }

        auto task = std::async(std::launch::async, [this, clientSocket]() {
            try {
                HandleClient(clientSocket);
            } catch (const std::exception& ex) {
                Logger::Error("Unhandled exception in client handler: {}", ex.what());
                closesocket(clientSocket);
            } catch (...) {
                Logger::Error("Unhandled non-standard exception in client handler");
                closesocket(clientSocket);
            }

            {
                std::lock_guard<std::mutex> lock(m_activeClientSocketsMutex);
                m_activeClientSockets.erase(clientSocket);
            }
            {
                std::lock_guard<std::mutex> sseLock(m_sseClientSocketsMutex);
                m_sseClientSockets.erase(clientSocket);
            }
        });

        {
            std::lock_guard<std::mutex> lock(m_clientTasksMutex);
            m_clientTasks.emplace_back(std::move(task));
        }
    }
}

void MCPHttpServer::CleanupFinishedClientTasks() {
    std::lock_guard<std::mutex> lock(m_clientTasksMutex);
    auto it = m_clientTasks.begin();
    while (it != m_clientTasks.end()) {
        if (!it->valid()) {
            it = m_clientTasks.erase(it);
            continue;
        }

        if (it->wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            try {
                it->get();
            } catch (const std::exception& ex) {
                Logger::Error("Client task ended with exception: {}", ex.what());
            } catch (...) {
                Logger::Error("Client task ended with unknown exception");
            }
            it = m_clientTasks.erase(it);
            continue;
        }

        ++it;
    }
}

void MCPHttpServer::HandleClient(SOCKET clientSocket) {
    std::string request;
    bool payloadTooLarge = false;
    if (!ReceiveHttpRequest(clientSocket, request, payloadTooLarge)) {
        if (payloadTooLarge) {
            SendHttpResponse(clientSocket, 413, "{\"error\":\"Payload Too Large\"}");
        } else if (!request.empty()) {
            SendHttpResponse(clientSocket, 400, "{\"error\":\"Bad Request\"}");
        }
        closesocket(clientSocket);
        return;
    }

    HandleHttpRequest(clientSocket, request);
    closesocket(clientSocket);
}

void MCPHttpServer::HandleHttpRequest(SOCKET clientSocket, const std::string& request) {
    std::string method, path, body;

    if (!ParseHttpRequest(request, method, path, body)) {
        SendHttpResponse(clientSocket, 400, "{\"error\":\"Bad Request\"}");
        return;
    }

    Logger::Debug("HTTP Request: " + method + " " + path);

    const std::string origin = GetHttpHeader(request, "Origin");
    const std::string host = GetHttpHeader(request, "Host");

    // Validate untrusted headers before reflecting Origin in any response.
    if (!ValidateCrossOriginAndHost(origin, host)) {
        SendHttpResponse(clientSocket, 403, "{\"error\":\"Forbidden\"}");
        return;
    }

    // Rate limiting: reject excessive requests (DoS protection).
    if (!g_rateLimiter.Allow()) {
        SendHttpResponse(clientSocket, 429, "{\"error\":\"Too Many Requests\"}", "application/json", origin);
        return;
    }

    // CORS preflight for browser-based clients (e.g. MCP Inspector Web UI).
    if (method == "OPTIONS" && (path == "/mcp" || path == "/mcp/" ||
        path == "/sse" || path == "/message" || path == "/messages" ||
        path == "/rpc" || path == "/rpc/" || path == "/")) {
        std::ostringstream resp;
        resp << "HTTP/1.1 204 No Content\r\n"
             << "Content-Length: 0\r\n";
        if (!origin.empty()) {
            resp << "Access-Control-Allow-Origin: " << origin << "\r\n"
                 << "Vary: Origin\r\n";
        }
        resp << "Access-Control-Allow-Methods: POST, GET, DELETE, OPTIONS\r\n"
             << "Access-Control-Allow-Headers: Content-Type, Accept, Authorization, "
                "Mcp-Protocol-Version, Mcp-Session-Id, Last-Event-ID\r\n"
             << "Access-Control-Max-Age: 86400\r\n"
             << "Connection: close\r\n\r\n";
        SendAll(clientSocket, resp.str());
        return;
    }

    if (method == "GET" && path == "/sse") {
        HandleSSE(clientSocket, origin);
    }
    else if (method == "POST" && (path == "/mcp" || path == "/mcp/")) {
        // Streamable HTTP transport (MCP 2025-03-26): single MCP endpoint.
        HandleStreamableHttpPost(clientSocket, body, origin);
    }
    else if (method == "GET" && (path == "/mcp" || path == "/mcp/")) {
        // Streamable HTTP GET stream for server-initiated notifications.
        HandleStreamableHttpStream(clientSocket, origin);
    }
    else if (method == "DELETE" && (path == "/mcp" || path == "/mcp/")) {
        // Stateless server: clients cannot terminate sessions explicitly.
        SendHttpResponse(clientSocket, 405,
            "{\"error\":\"Method Not Allowed\",\"reason\":\"stateless server\"}", "application/json", origin);
    }
    else if (method == "POST" &&
             (path == "/message" || path == "/" || path == "/messages" ||
              path == "/rpc" || path == "/rpc/")) {
        // 鏀寔澶氱 POST 璺緞锛?, /message, /messages
        HandlePostMessage(clientSocket, body, origin);
    }
    else if (method == "GET" && path == "/") {
        // 鍋ュ悍妫€鏌?
        SendHttpResponse(clientSocket, 200, "{\"status\":\"ok\",\"service\":\"x64dbg-mcp\"}", "application/json", origin);
    }
    else {
        // Escape path for JSON safety to prevent injection
        std::string safePath = path;
        for (size_t i = 0; i < safePath.size(); ) {
            if (safePath[i] == '"' || safePath[i] == '\\') {
                safePath.insert(i, "\\");
                i += 2;
            } else {
                ++i;
            }
        }
        SendHttpResponse(clientSocket, 404,
            "{\"error\":\"Not Found\",\"path\":\"" + safePath + "\"}", "application/json", origin);
    }
}

void MCPHttpServer::HandleSSE(SOCKET clientSocket, const std::string& origin) {
    // 鍙戦€?SSE 鍝嶅簲澶?
    std::ostringstream headerStream;
    headerStream << "HTTP/1.1 200 OK\r\n"
                 << "Content-Type: text/event-stream; charset=utf-8\r\n"
                 << "Cache-Control: no-cache\r\n";
    if (!origin.empty()) {
        headerStream << "Access-Control-Allow-Origin: " << origin << "\r\n"
                     << "Vary: Origin\r\n";
    }
    headerStream << "Connection: keep-alive\r\n\r\n";
    const std::string headers = headerStream.str();

    if (!SendAll(clientSocket, headers)) {
        Logger::Error("Failed to send SSE headers");
        return;
    }

    // MCP SSE transport handshake: immediately announce the POST endpoint.
    {
        const std::string endpointEvent =
            "event: endpoint\r\n"
            "data: /message\r\n"
            "\r\n";
        std::lock_guard<std::mutex> lock(m_sseSendMutex);
        if (!SendAll(clientSocket, endpointEvent)) {
            Logger::Error("Failed to send SSE endpoint event");
            return;
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_sseClientSocketsMutex);
        m_sseClientSockets.insert(clientSocket);
    }

    Logger::Info("SSE connection established, endpoint event sent");

    // 璁剧疆 socket 涓洪潪闃诲妯″紡
    u_long mode = 1;
    ioctlsocket(clientSocket, FIONBIO, &mode);

    // 璇诲彇瀹㈡埛绔彂閫佺殑娑堟伅锛堟湁浜?MCP 瀹㈡埛绔€氳繃 SSE 杩炴帴鍙戦€佽姹傦級
    char buffer[4096];
    std::string accumulated;
    int heartbeatCounter = 0;
    
    while (m_running) {
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            accumulated += buffer;

            // Prevent unbounded buffer growth (DoS protection).
            constexpr size_t kMaxSseAccumulatedBytes = 1024 * 1024; // 1 MB
            if (accumulated.size() > kMaxSseAccumulatedBytes) {
                Logger::Warning("SSE client buffer exceeded {} bytes, disconnecting",
                                kMaxSseAccumulatedBytes);
                break;
            }

            // 鏌ユ壘瀹屾暣鐨?JSON 娑堟伅锛堟寜琛屽垎闅旓級
            size_t pos;
            while ((pos = accumulated.find('\n')) != std::string::npos) {
                std::string line = accumulated.substr(0, pos);
                accumulated = accumulated.substr(pos + 1);
                
                // 璺宠繃绌鸿
                if (line.empty() || line == "\r") continue;
                
                Logger::Debug("SSE received: " + line);
                
                // 瑙ｆ瀽骞跺鐞?JSON-RPC 璇锋眰
                std::string method, requestId;
                bool hasRequestId = false;
                if (ParseJsonRpcRequest(line, method, requestId, hasRequestId)) {
                    std::string response = HandleMCPMethod(method, requestId, line);
                    if (hasRequestId && !response.empty()) {
                        SendSSEEvent(clientSocket, "message", response);
                    }
                } else {
                    json errorResponse;
                    try {
                        const json requestJson = json::parse(line);
                        json idValue = nullptr;
                        if (requestJson.is_object() && requestJson.contains("id")) {
                            const auto& id = requestJson["id"];
                            if (id.is_null() || id.is_string() || id.is_number_integer()) {
                                idValue = id;
                            }
                        }

                        errorResponse = {
                            {"jsonrpc", "2.0"},
                            {"id", idValue},
                            {"error", {
                                {"code", -32600},
                                {"message", "Invalid Request"}
                            }}
                        };
                    } catch (const json::exception&) {
                        errorResponse = {
                            {"jsonrpc", "2.0"},
                            {"id", nullptr},
                            {"error", {
                                {"code", -32700},
                                {"message", "Parse error"}
                            }}
                        };
                    }

                    SendSSEEvent(clientSocket, "message", errorResponse.dump());
                }
            }
        } else if (bytesReceived == 0) {
            // 瀹㈡埛绔甯稿叧闂?
            Logger::Debug("SSE client disconnected");
            break;
        } else {
            // WSAEWOULDBLOCK 琛ㄧず娌℃湁鏁版嵁鍙锛岃繖鏄甯哥殑
            int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK) {
                Logger::Error("SSE recv error: " + std::to_string(error));
                break;
            }
        }
        
        // 姣?15 绉掑彂閫佷竴娆″績璺筹紙淇濇寔杩炴帴娲昏穬锛?
        if (++heartbeatCounter >= 150) {  // 150 * 100ms = 15s
            SendSSEEvent(clientSocket, "ping", "{}");
            heartbeatCounter = 0;
        }
        
        // 鐭殏浼戠湢閬垮厤 CPU 鍗犵敤杩囬珮
        Sleep(100);
    }
    
    // SSE 杩炴帴缁撴潫锛屽叧闂?socket
    Logger::Info("Closing SSE connection");
    {
        std::lock_guard<std::mutex> lock(m_sseClientSocketsMutex);
        m_sseClientSockets.erase(clientSocket);
    }
}

void MCPHttpServer::HandlePostMessage(SOCKET clientSocket, const std::string& body,
                                      const std::string& origin) {
    Logger::Debug("POST body received: " + body);

    try {
        [[maybe_unused]] const auto parsed = json::parse(body);
    } catch (const json::exception&) {
        Logger::Error("Failed to parse JSON body");
        SendHttpResponse(clientSocket, 400,
                         "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32700,\"message\":\"Parse error\"}}",
                         "application/json", origin);
        return;
    }

    std::string method, requestId;
    bool hasRequestId = false;
    if (!ParseJsonRpcRequest(body, method, requestId, hasRequestId)) {
        Logger::Error("Invalid JSON-RPC request");
        SendHttpResponse(clientSocket, 400,
                         "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32600,\"message\":\"Invalid Request\"}}",
                         "application/json", origin);
        return;
    }

    Logger::Info("MCP Method: " + method + ", ID: " + requestId);

    // Check whether at least one SSE client is currently connected.
    // MCP SSE transport delivers the response over SSE, not the POST reply.
    bool hasSseClient = false;
    {
        std::lock_guard<std::mutex> lock(m_sseClientSocketsMutex);
        hasSseClient = !m_sseClientSockets.empty();
    }

    std::string response = HandleMCPMethod(method, requestId, body);

    // Notification (no id) or methods with no payload: 202 either way.
    if (!hasRequestId || response.empty()) {
        SendHttpResponse(clientSocket, 202, "", "application/json", origin);
        return;
    }

    if (hasSseClient) {
        // MCP SSE contract: ack the POST with 202, push response via SSE.
        SendHttpResponse(clientSocket, 202, "", "application/json", origin);
        BroadcastSSEEvent("message", response);
        Logger::Debug("Response dispatched via SSE: " + response);
    } else {
        // Fallback for plain HTTP clients (no SSE attached): reply inline.
        Logger::Debug("Inline reply (no SSE client): " + response);
        SendHttpResponse(clientSocket, 200, response, "application/json", origin);
    }
}

bool MCPHttpServer::ParseJsonRpcRequest(const std::string& rawJson,
                                        std::string& method,
                                        std::string& requestId,
                                        bool& hasRequestId) {
    method.clear();
    requestId = "null";
    hasRequestId = false;

    try {
        json request = json::parse(rawJson);
        if (!request.is_object()) {
            return false;
        }

        auto versionIt = request.find("jsonrpc");
        if (versionIt == request.end()) {
            return false;
        }
        if (!versionIt->is_string() || versionIt->get<std::string>() != "2.0") {
            return false;
        }

        auto methodIt = request.find("method");
        if (methodIt == request.end() || !methodIt->is_string()) {
            return false;
        }
        method = methodIt->get<std::string>();

        auto idIt = request.find("id");
        if (idIt != request.end()) {
            hasRequestId = true;
            if (idIt->is_null()) {
                requestId = "null";
            } else if (idIt->is_string() || idIt->is_number_integer()) {
                requestId = idIt->dump();
            } else {
                return false;
            }
        }

        return true;
    } catch (const json::exception&) {
        return false;
    }
}

std::string MCPHttpServer::HandleMCPMethod(const std::string& method, const std::string& requestId, const std::string& body) {
    if (method == "initialize") {
        Logger::Info("Handling initialize request");
        return "{\"jsonrpc\":\"2.0\",\"id\":" + requestId +
               ",\"result\":{\"protocolVersion\":\"2024-11-05\","
               "\"capabilities\":{\"tools\":{},\"resources\":{},\"prompts\":{}},"
               "\"serverInfo\":{\"name\":\"x64dbg-mcp\",\"version\":\"1.0.9\"}}}";
    }
    else if (method == "notifications/initialized") {
        // 杩欐槸瀹㈡埛绔彂鐨勯€氱煡锛屼笉闇€瑕佸搷搴?
        Logger::Debug("Received initialized notification from client");
        return ""; // 涓嶈繑鍥炲搷搴?
    }
    else if (method == "tools/list") {
        auto& registry = MCPToolRegistry::Instance();
        json result = registry.GenerateToolsListResponse();
        
        Logger::Info("Handling tools/list request, have {} tools", result["tools"].size());
        
        json response = {
            {"jsonrpc", "2.0"},
            {"id", SafeParseId(requestId)},
            {"result", result}
        };
        
        return response.dump();
    }
    else if (method == "tools/call") {
        Logger::Info("Handling tools/call request");
        
        try {
            // 瑙ｆ瀽璇锋眰 body
            json requestJson = json::parse(body);
            
            if (!requestJson.contains("params") || !requestJson["params"].is_object()) {
                return json({
                    {"jsonrpc", "2.0"},
                    {"id", SafeParseId(requestId)},
                    {"error", {
                        {"code", -32602},
                        {"message", "Invalid params: missing params object"}
                    }}
                }).dump();
            }
            
            json params = requestJson["params"];
            
            if (!params.contains("name") || !params["name"].is_string()) {
                return json({
                    {"jsonrpc", "2.0"},
                    {"id", SafeParseId(requestId)},
                    {"error", {
                        {"code", -32602},
                        {"message", "Invalid params: missing or invalid tool name"}
                    }}
                }).dump();
            }
            
            std::string toolName = params["name"].get<std::string>();
            if (params.contains("arguments") && !params["arguments"].is_object()) {
                return json({
                    {"jsonrpc", "2.0"},
                    {"id", SafeParseId(requestId)},
                    {"error", {
                        {"code", -32602},
                        {"message", "Invalid params: arguments must be an object"}
                    }}
                }).dump();
            }
            json arguments = params.value("arguments", json::object());
            
            Logger::Info("Calling tool: {} with args: {}", toolName, arguments.dump());
            
            // 璋冪敤宸ュ叿
            MCPToolCallResult toolResult = CallMCPTool(toolName, arguments);

            if (!toolResult.success) {
                return json({
                    {"jsonrpc", "2.0"},
                    {"id", SafeParseId(requestId)},
                    {"error", {
                        {"code", toolResult.errorCode},
                        {"message", toolResult.errorMessage}
                    }}
                }).dump();
            }
            
            return json({
                {"jsonrpc", "2.0"},
                {"id", SafeParseId(requestId)},
                {"result", {
                    {"content", json::array({
                        {
                            {"type", "text"},
                            {"text", toolResult.contentText}
                        }
                    })}
                }}
            }).dump();
            
        } catch (const json::exception& e) {
            Logger::Error("Invalid params in tools/call: {}", e.what());
            return json({
                {"jsonrpc", "2.0"},
                {"id", SafeParseId(requestId)},
                {"error", {
                    {"code", -32602},
                    {"message", std::string("Invalid params: ") + e.what()}
                }}
            }).dump();
        } catch (const std::exception& e) {
            Logger::Error("Exception in tools/call: {}", e.what());
            return json({
                {"jsonrpc", "2.0"},
                {"id", SafeParseId(requestId)},
                {"error", {
                    {"code", -32603},
                    {"message", std::string("Internal error: ") + e.what()}
                }}
            }).dump();
        }
    }
    // Resources API
    else if (method == "resources/list") {
        auto& registry = MCPResourceRegistry::Instance();
        json result = registry.GenerateResourcesListResponse();
        
        Logger::Info("Handling resources/list request, have {} resources", result["resources"].size());
        
        json response = {
            {"jsonrpc", "2.0"},
            {"id", SafeParseId(requestId)},
            {"result", result}
        };
        
        return response.dump();
    }
    else if (method == "resources/templates/list") {
        auto& registry = MCPResourceRegistry::Instance();
        json result = registry.GenerateTemplatesListResponse();
        
        Logger::Info("Handling resources/templates/list request, have {} templates", 
                     result["resourceTemplates"].size());
        
        json response = {
            {"jsonrpc", "2.0"},
            {"id", SafeParseId(requestId)},
            {"result", result}
        };
        
        return response.dump();
    }
    else if (method == "resources/read") {
        Logger::Info("Handling resources/read request");
        
        try {
            json requestJson = json::parse(body);
            
            if (!requestJson.contains("params") || !requestJson["params"].is_object()) {
                return json({
                    {"jsonrpc", "2.0"},
                    {"id", SafeParseId(requestId)},
                    {"error", {
                        {"code", -32602},
                        {"message", "Invalid params: missing params object"}
                    }}
                }).dump();
            }

            const json& params = requestJson["params"];
            if (!params.contains("uri") || !params["uri"].is_string()) {
                return json({
                    {"jsonrpc", "2.0"},
                    {"id", SafeParseId(requestId)},
                    {"error", {
                        {"code", -32602},
                        {"message", "Invalid params: missing or invalid uri"}
                    }}
                }).dump();
            }
            
            std::string uri = params["uri"].get<std::string>();
            Logger::Info("Reading resource: {}", uri);
            
            auto& registry = MCPResourceRegistry::Instance();
            MCPResourceContent content = registry.ReadResource(uri);
            
            json response = {
                {"jsonrpc", "2.0"},
                {"id", SafeParseId(requestId)},
                {"result", {
                    {"contents", json::array({content.ToMCPFormat()})}
                }}
            };
            
            return response.dump();
            
        } catch (const json::exception& e) {
            Logger::Error("Invalid params in resources/read: {}", e.what());
            return json({
                {"jsonrpc", "2.0"},
                {"id", SafeParseId(requestId)},
                {"error", {
                    {"code", -32602},
                    {"message", std::string("Invalid params: ") + e.what()}
                }}
            }).dump();
        } catch (const std::exception& e) {
            Logger::Error("Exception in resources/read: {}", e.what());
            return json({
                {"jsonrpc", "2.0"},
                {"id", SafeParseId(requestId)},
                {"error", {
                    {"code", -32603},
                    {"message", std::string("Internal error: ") + e.what()}
                }}
            }).dump();
        }
    }
    // Prompts API
    else if (method == "prompts/list") {
        auto& registry = MCPPromptRegistry::Instance();
        json result = registry.GeneratePromptsListResponse();
        
        Logger::Info("Handling prompts/list request, have {} prompts", result["prompts"].size());
        
        json response = {
            {"jsonrpc", "2.0"},
            {"id", SafeParseId(requestId)},
            {"result", result}
        };
        
        return response.dump();
    }
    else if (method == "prompts/get") {
        Logger::Info("Handling prompts/get request");
        
        try {
            json requestJson = json::parse(body);
            
            if (!requestJson.contains("params") || !requestJson["params"].is_object()) {
                return json({
                    {"jsonrpc", "2.0"},
                    {"id", SafeParseId(requestId)},
                    {"error", {
                        {"code", -32602},
                        {"message", "Invalid params: missing params object"}
                    }}
                }).dump();
            }

            const json& params = requestJson["params"];
            if (!params.contains("name") || !params["name"].is_string()) {
                return json({
                    {"jsonrpc", "2.0"},
                    {"id", SafeParseId(requestId)},
                    {"error", {
                        {"code", -32602},
                        {"message", "Invalid params: missing or invalid name"}
                    }}
                }).dump();
            }

            if (params.contains("arguments") && !params["arguments"].is_object()) {
                return json({
                    {"jsonrpc", "2.0"},
                    {"id", SafeParseId(requestId)},
                    {"error", {
                        {"code", -32602},
                        {"message", "Invalid params: arguments must be an object"}
                    }}
                }).dump();
            }
            
            std::string promptName = params["name"].get<std::string>();
            json arguments = params.value("arguments", json::object());
            
            Logger::Info("Getting prompt: {} with args: {}", promptName, arguments.dump());
            
            auto& registry = MCPPromptRegistry::Instance();
            MCPPromptResult promptResult = registry.GetPrompt(promptName, arguments);
            
            json response = {
                {"jsonrpc", "2.0"},
                {"id", SafeParseId(requestId)},
                {"result", promptResult.ToMCPFormat()}
            };
            
            return response.dump();
            
        } catch (const json::exception& e) {
            Logger::Error("Invalid params in prompts/get: {}", e.what());
            return json({
                {"jsonrpc", "2.0"},
                {"id", SafeParseId(requestId)},
                {"error", {
                    {"code", -32602},
                    {"message", std::string("Invalid params: ") + e.what()}
                }}
            }).dump();
        } catch (const std::exception& e) {
            Logger::Error("Exception in prompts/get: {}", e.what());
            return json({
                {"jsonrpc", "2.0"},
                {"id", SafeParseId(requestId)},
                {"error", {
                    {"code", -32603},
                    {"message", std::string("Internal error: ") + e.what()}
                }}
            }).dump();
        }
    }
    else {
        return "{\"jsonrpc\":\"2.0\",\"id\":" + requestId + 
               ",\"error\":{\"code\":-32601,\"message\":\"Method not found\"}}";
    }
}

bool MCPHttpServer::ParseHttpRequest(const std::string& request, 
                                     std::string& method, 
                                     std::string& path, 
                                     std::string& body) {
    // 瑙ｆ瀽璇锋眰琛?
    size_t firstSpace = request.find(' ');
    if (firstSpace == std::string::npos) return false;
    
    method = request.substr(0, firstSpace);
    
    size_t secondSpace = request.find(' ', firstSpace + 1);
    if (secondSpace == std::string::npos) return false;
    
    path = request.substr(firstSpace + 1, secondSpace - firstSpace - 1);

    // Normalize route path by dropping query parameters.
    const size_t queryPos = path.find('?');
    if (queryPos != std::string::npos) {
        path = path.substr(0, queryPos);
    }
    
    // 鎻愬彇 body锛堝湪 \r\n\r\n 涔嬪悗锛?
    size_t bodyStart = request.find("\r\n\r\n");
    if (bodyStart != std::string::npos) {
        body = request.substr(bodyStart + 4);
    }
    
    return true;
}

bool MCPHttpServer::ValidateCrossOriginAndHost(const std::string& origin, const std::string& host) {
    auto& config = ConfigManager::Instance();

    // Origin header is always sent by browsers for cross-origin requests.
    // If present, it must be in the configured allowlist.
    if (!origin.empty()) {
        const json allowlist = config.GetOriginAllowlist();
        bool allowed = false;
        if (allowlist.is_array()) {
            for (const auto& entry : allowlist) {
                if (entry.is_string() && entry.get<std::string>() == origin) {
                    allowed = true;
                    break;
                }
            }
        }
        if (!allowed) {
            Logger::Warning("[Security] Rejected cross-origin POST from Origin: {}", origin);
            return false;
        }
    }

    // Host header validation for DNS-rebinding defense.
    // The Host must match the configured bind address, a loopback address, or an explicit allowlist entry.
    if (!host.empty()) {
        std::string hostOnly = host;
        if (!hostOnly.empty() && hostOnly.front() == '[') {
            const size_t closingBracket = hostOnly.find(']');
            if (closingBracket != std::string::npos) {
                hostOnly = hostOnly.substr(1, closingBracket - 1);
            }
        } else {
            const size_t colonPos = hostOnly.find(':');
            if (colonPos != std::string::npos) {
                hostOnly = hostOnly.substr(0, colonPos);
            }
        }
        hostOnly = ToLowerCopy(hostOnly);

        bool valid = (hostOnly == "127.0.0.1" || hostOnly == "localhost" ||
                      hostOnly == "::1" || hostOnly == ToLowerCopy(m_host));
        if (!valid) {
            const json allowlist = config.GetHostAllowlist();
            if (allowlist.is_array()) {
                for (const auto& entry : allowlist) {
                    if (entry.is_string() &&
                        ToLowerCopy(entry.get<std::string>()) == hostOnly) {
                        valid = true;
                        break;
                    }
                }
            }
        }
        if (!valid) {
            Logger::Warning("[Security] Rejected POST with unexpected Host: {}", host);
            return false;
        }
    }

    return true;
}

void MCPHttpServer::SendHttpResponse(SOCKET socket, int statusCode,
                                     const std::string& body,
                                     const std::string& contentType,
                                     const std::string& origin) {
    const std::string responseBody = (statusCode == 204) ? "" : body;
    const char* statusText = GetHttpStatusText(statusCode);
    std::string responseContentType = contentType;
    const bool missingCharset = responseContentType.find("charset=") == std::string::npos;
    const bool isTextual = responseContentType.rfind("text/", 0) == 0 ||
                           responseContentType.rfind("application/json", 0) == 0;
    if (missingCharset && isTextual) {
        responseContentType += "; charset=utf-8";
    }
    
    std::ostringstream response;
    response << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n"
             << "Content-Type: " << responseContentType << "\r\n"
             << "Content-Length: " << responseBody.length() << "\r\n";
    if (!origin.empty()) {
        response << "Access-Control-Allow-Origin: " << origin << "\r\n"
                 << "Vary: Origin\r\n";
    }
    response << "Connection: close\r\n"
             << "\r\n"
             << responseBody;
    
    std::string responseStr = response.str();
    if (!SendAll(socket, responseStr)) {
        Logger::Error("Failed to send HTTP response");
    }
}

bool MCPHttpServer::SendSSEEvent(SOCKET socket, const std::string& event, const std::string& data) {
    std::ostringstream sse;
    sse << "event: " << event << "\r\n"
        << "data: " << data << "\r\n"
        << "\r\n";
    
    std::string sseStr = sse.str();
    std::lock_guard<std::mutex> lock(m_sseSendMutex);
    if (!SendAll(socket, sseStr)) {
        Logger::Debug("Failed to send SSE event '{}'", event);
        return false;
    }

    return true;
}

bool MCPHttpServer::BroadcastSSEEvent(const std::string& event, const std::string& data) {
    std::vector<SOCKET> targets;
    {
        std::lock_guard<std::mutex> lock(m_sseClientSocketsMutex);
        targets.assign(m_sseClientSockets.begin(), m_sseClientSockets.end());
    }

    if (targets.empty()) {
        return false;
    }

    std::vector<SOCKET> disconnected;
    bool sentAtLeastOne = false;
    for (SOCKET clientSocket : targets) {
        if (SendSSEEvent(clientSocket, event, data)) {
            sentAtLeastOne = true;
            continue;
        }
        disconnected.push_back(clientSocket);
    }

    if (!disconnected.empty()) {
        std::lock_guard<std::mutex> lock(m_sseClientSocketsMutex);
        for (SOCKET socket : disconnected) {
            m_sseClientSockets.erase(socket);
        }
    }

    return sentAtLeastOne;
}

MCPHttpServer::MCPToolCallResult MCPHttpServer::CallMCPTool(const std::string& toolName, const nlohmann::json& arguments) {
    auto& registry = MCPToolRegistry::Instance();
    MCPToolCallResult result;
    
    // 鏌ユ壘宸ュ叿瀹氫箟
    auto toolOpt = registry.FindTool(toolName);
    if (!toolOpt.has_value()) {
        Logger::Error("Tool not found: {}", toolName);
        result.errorCode = -32601;
        result.errorMessage = "Tool not found: " + toolName;
        return result;
    }
    
    const MCPToolDefinition& tool = toolOpt.value();
    
    // 楠岃瘉鍙傛暟
    std::string validationError = tool.ValidateArguments(arguments);
    if (!validationError.empty()) {
        Logger::Error("Tool argument validation failed: {}", validationError);
        result.errorCode = -32602;
        result.errorMessage = validationError;
        return result;
    }
    
    Logger::Info("Executing tool: {} -> method: {}", toolName, tool.jsonrpcMethod);
    
    // 鏋勫缓 JSON-RPC 璇锋眰鍙戦€佺粰 MethodDispatcher
    try {
        auto& dispatcher = MethodDispatcher::Instance();
        
        // 鏋勫缓璇锋眰
        JSONRPCRequest request;
        request.jsonrpc = "2.0";
        request.method = tool.jsonrpcMethod;
        request.id = ++m_requestId;
        request.params = tool.TransformToJSONRPC(arguments);
        
        // 璋冪敤鍒嗗彂鍣?
        JSONRPCResponse response = dispatcher.Dispatch(request);
        
        if (response.error.has_value()) {
            Logger::Error("Tool execution failed: {}", response.error->message);
            result.errorCode = response.error->code;
            result.errorMessage = response.error->message;
            return result;
        }
        
        // 杩斿洖鏍煎紡鍖栫殑缁撴灉
        result.contentText = response.result.dump(2);  // Pretty output
        result.success = true;
        return result;
        
    } catch (const std::exception& e) {
        Logger::Error("Exception calling tool: {}", e.what());
        result.success = false;
        result.errorCode = -32603;
        result.errorMessage = std::string("Internal error: ") + e.what();
        return result;
    }
}


// =============================================================================
// Streamable HTTP transport (MCP 2025-03-26)
//
// Single MCP endpoint at /mcp. POST replies with the JSON-RPC response inline
// as application/json (or 202 for notifications). GET opens a long-lived SSE
// stream solely for server-initiated notifications (no endpoint handshake).
// Stateless: no Mcp-Session-Id, no DELETE-driven session termination.
// =============================================================================

void MCPHttpServer::HandleStreamableHttpPost(SOCKET clientSocket, const std::string& body, const std::string& origin) {
    Logger::Debug("[Streamable HTTP] POST body: " + body);

    try {
        [[maybe_unused]] const auto parsed = nlohmann::json::parse(body);
    } catch (const nlohmann::json::exception&) {
        Logger::Error("[Streamable HTTP] Failed to parse JSON body");
        SendHttpResponse(clientSocket, 400,
            "{\"jsonrpc\":\"2.0\",\"id\":null,\"error\":{\"code\":-32700,\"message\":\"Parse error\"}}",
            "application/json", origin);
        return;
    }

    std::string method;
    std::string requestId;
    bool hasRequestId = false;
    if (!ParseJsonRpcRequest(body, method, requestId, hasRequestId)) {
        Logger::Error("[Streamable HTTP] Invalid JSON-RPC request");
        SendHttpResponse(clientSocket, 400,
            "{\"jsonrpc\":\"2.0\",\"id\":null,\"error\":{\"code\":-32600,\"message\":\"Invalid Request\"}}",
            "application/json", origin);
        return;
    }

    Logger::Info("[Streamable HTTP] Method: " + method + ", ID: " + requestId);

    std::string response = HandleMCPMethod(method, requestId, body);

    if (!hasRequestId || response.empty()) {
        // Notification (no id) or fire-and-forget method: 202 Accepted, no body.
        SendHttpResponse(clientSocket, 202, "", "application/json", origin);
        return;
    }

    // Request: return JSON-RPC response inline as application/json.
    SendHttpResponse(clientSocket, 200, response, "application/json", origin);
}

void MCPHttpServer::HandleStreamableHttpStream(SOCKET clientSocket,
                                               const std::string& origin) {
    std::ostringstream headerStream;
    headerStream << "HTTP/1.1 200 OK\r\n"
                 << "Content-Type: text/event-stream; charset=utf-8\r\n"
                 << "Cache-Control: no-cache\r\n";
    if (!origin.empty()) {
        headerStream << "Access-Control-Allow-Origin: " << origin << "\r\n"
                     << "Vary: Origin\r\n";
    }
    headerStream << "Connection: keep-alive\r\n\r\n";
    const std::string headers = headerStream.str();

    if (!SendAll(clientSocket, headers)) {
        Logger::Error("[Streamable HTTP] Failed to send stream headers");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_sseClientSocketsMutex);
        m_sseClientSockets.insert(clientSocket);
    }

    Logger::Info("[Streamable HTTP] GET stream established");

    u_long mode = 1;
    ioctlsocket(clientSocket, FIONBIO, &mode);

    char buffer[256];
    int heartbeatCounter = 0;

    while (m_running) {
        const int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived == 0) {
            Logger::Debug("[Streamable HTTP] Client closed GET stream");
            break;
        }
        if (bytesReceived < 0) {
            const int err = WSAGetLastError();
            if (err != WSAEWOULDBLOCK) {
                Logger::Debug("[Streamable HTTP] recv error: " + std::to_string(err));
                break;
            }
        }

        // SSE comment line as a no-op heartbeat (~15s).
        if (++heartbeatCounter >= 150) {
            const std::string ping = ": ping\r\n\r\n";
            std::lock_guard<std::mutex> lock(m_sseSendMutex);
            if (!SendAll(clientSocket, ping)) {
                Logger::Debug("[Streamable HTTP] Heartbeat send failed, closing");
                break;
            }
            heartbeatCounter = 0;
        }

        Sleep(100);
    }

    Logger::Info("[Streamable HTTP] Closing GET stream");
    {
        std::lock_guard<std::mutex> lock(m_sseClientSocketsMutex);
        m_sseClientSockets.erase(clientSocket);
    }
}

} // namespace MCP
