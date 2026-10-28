#include "communication/MCPHttpServer.h"
#include "core/ConfigManager.h"
#include "core/MethodDispatcher.h"
#include <ws2tcpip.h>
#include <stdexcept>
#include <iostream>

namespace MCP {
MethodDispatcher& MethodDispatcher::Instance() { static MethodDispatcher instance; return instance; }
JSONRPCResponse MethodDispatcher::Dispatch(const JSONRPCRequest& request) {
    return {"2.0", request.id, {{"method", request.method}}, std::nullopt};
}
}

namespace {
int port;
SOCKET Connect() {
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<u_short>(port));
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    if (connect(s, reinterpret_cast<sockaddr*>(&address), sizeof(address))) throw std::runtime_error("connect");
    DWORD timeout = 1500;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&timeout), sizeof(timeout));
    return s;
}
std::string Read(SOCKET s) {
    char buffer[8192];
    int n = recv(s, buffer, sizeof(buffer), 0);
    return n > 0 ? std::string(buffer, n) : "";
}
std::string OpenStream(SOCKET s) {
    const std::string request = "GET /sse HTTP/1.1\r\nHost: localhost\r\n\r\n";
    send(s, request.data(), static_cast<int>(request.size()), 0);
    std::string data;
    while (data.find("/message?sessionId=") == std::string::npos || data.rfind("\r\n\r\n") < data.find("/message?sessionId=")) {
        const auto part = Read(s);
        if (part.empty()) throw std::runtime_error("SSE handshake");
        data += part;
    }
    const auto start = data.find("/message?sessionId=");
    return data.substr(start, data.find("\r\n", start) - start);
}
std::string Post(const std::string& path, const std::string& body = R"({"jsonrpc":"2.0","id":73,"method":"initialize"})", const std::string& extraHeaders = "") {
    SOCKET s = Connect();
    const std::string request = "POST " + path + " HTTP/1.1\r\nHost: localhost\r\nContent-Length: " + std::to_string(body.size()) + "\r\n" + extraHeaders + "\r\n" + body;
    send(s, request.data(), static_cast<int>(request.size()), 0);
    std::string response, part;
    while (!(part = Read(s)).empty()) response += part;
    closesocket(s);
    return response;
}
}

int main() {
    MCP::ConfigManager::Instance().Set("server.max_connections", 4);
    MCP::ConfigManager::Instance().Set("timeout.request_timeout_ms", 150);
    MCP::MCPHttpServer server;
    const int firstPort = 20000 + static_cast<int>(GetCurrentProcessId() % 20000);
    for (port = firstPort; port < firstPort + 100; ++port) if (server.Start("127.0.0.1", port)) break;
    if (!server.IsRunning()) return 1;
    try {
        SOCKET first = Connect(), second = Connect();
        const auto endpoint = OpenStream(first), other = OpenStream(second);
        const auto reply = Post(endpoint);
        if (endpoint == other || reply.find("202") == std::string::npos) { std::cerr << endpoint << "\n" << other << "\n" << reply; return 2; }
        if (Read(first).find("\"id\":73") == std::string::npos || !Read(second).empty()) return 3;
        if (Post("/rpc").find("\"id\":73") == std::string::npos) return 4;
        if (Post("/message?sessionId=unknown").find("404") == std::string::npos) return 5;
        MCP::ConfigManager::Instance().Set("features.enable_batch_requests", false);
        const std::string batch = R"([{"jsonrpc":"2.0","id":81,"method":"initialize"},{"jsonrpc":"2.0","method":"notifications/initialized"}])";
        if (Post("/mcp", batch).find("-32600") == std::string::npos) return 11;
        MCP::ConfigManager::Instance().Set("features.enable_batch_requests", true);
        if (Post("/mcp", batch).find("\"id\":81") == std::string::npos) return 12;
        MCP::ConfigManager::Instance().Set("features.enable_notifications", false);
        if (MCP::MCPHttpServer::BroadcastNotification("test", json::object())) return 13;
        MCP::ConfigManager::Instance().Set("features.enable_notifications", true);
        if (!MCP::MCPHttpServer::BroadcastNotification("test", json::object()) || Read(first).find("test") == std::string::npos) return 14;
        Read(second);
        MCP::ConfigManager::Instance().Set("features.heartbeat_interval_seconds", 1);
        MCP::ConfigManager::Instance().Set("features.enable_heartbeat", true);
        if (Read(first).find("ping") == std::string::npos) return 15;
        MCP::ConfigManager::Instance().Set("features.enable_heartbeat", false);
        Read(first); // Drain any heartbeat already in flight.
        if (!Read(first).empty()) return 16;
        const std::string one = R"({"jsonrpc":"2.0","id":9,"method":"initialize"})";
        if (Post("/rpc", one, "Origin: https://blocked\r\n").find("403") == std::string::npos) return 17;
        if (Post("/rpc", one, "X-Origin: https://blocked\r\n").find("200") == std::string::npos) return 18;
        MCP::ConfigManager::Instance().Set("security.auth_enabled", true);
        MCP::ConfigManager::Instance().Set("security.auth_token", std::string("regression-token"));
        if (Post("/rpc", one, "Authorization: Bearer regression-token\r\n").find("200") == std::string::npos) return 19;
        if (Post("/rpc", one, "X-Authorization: Bearer regression-token\r\n").find("401") == std::string::npos) return 20;
        MCP::ConfigManager::Instance().Set("security.auth_enabled", false);
        closesocket(first); closesocket(second);
        server.Stop();
        if (!server.Start("127.0.0.1", port)) return 6;
        std::vector<SOCKET> stalled;
        for (int i = 0; i < 4; ++i) {
            auto socket = Connect();
            const std::string partial = "POST /rpc HTTP/1.1\r\nHost:";
            send(socket, partial.data(), static_cast<int>(partial.size()), 0);
            stalled.push_back(socket);
        }
        SOCKET rejected = Connect();
        if (!Read(rejected).empty()) return 7;
        closesocket(rejected);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        for (auto socket : stalled) {
            const auto response = Read(socket);
            if (response.find("400") == std::string::npos && !response.empty()) return 8;
            closesocket(socket);
        }
        if (Post("/rpc").find("200") == std::string::npos) return 9;
        SOCKET activeStream = Connect(); OpenStream(activeStream);
        std::atomic<bool> broadcasting{true};
        std::thread broadcaster([&] {
            while (broadcasting) {
                MCP::MCPHttpServer::BroadcastNotification("shutdown-test", json::object());
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
        server.Stop();
        broadcasting = false;
        broadcaster.join();
        closesocket(activeStream);
    } catch (const std::exception& ex) { std::cerr << ex.what(); return 10; }
    return 0;
}
