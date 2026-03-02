#include "TCPServer.h"
#include "../core/Logger.h"
#include "../core/ConfigManager.h"

#pragma comment(lib, "ws2_32.lib")

namespace MCP {

bool TCPServer::s_wsaInitialized = false;
std::mutex TCPServer::s_wsaMutex;

TCPServer::TCPServer()
    : m_listenSocket(INVALID_SOCKET)
    , m_running(false)
    , m_port(0)
{
    InitializeWSA();
}

TCPServer::~TCPServer() {
    Stop();
}

bool TCPServer::Start(const std::string& address, uint16_t port) {
    if (m_running) {
        Logger::Warning("Server is already running");
        return false;
    }
    
    m_address = address;
    m_port = port;
    
    // 閸掓稑缂撻惄鎴濇儔 socket
    m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listenSocket == INVALID_SOCKET) {
        Logger::Error("Failed to create socket: {}", WSAGetLastError());
        return false;
    }
    
    // 鐠佸墽鐤?socket 闁銆嶉敍鍫濆帒鐠佺婀撮崸鈧柌宥囨暏閿?
    int optval = 1;
    setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR, 
              reinterpret_cast<const char*>(&optval), sizeof(optval));
    
    // 缂佹垵鐣鹃崷鏉挎絻
    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    if (address == "0.0.0.0" || address == "*") {
        serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else if (inet_pton(AF_INET, address.c_str(), &serverAddr.sin_addr) != 1) {
        addrinfo hints = {};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        addrinfo* result = nullptr;
        const int resolveResult = getaddrinfo(address.c_str(), nullptr, &hints, &result);
        if (resolveResult != 0 || result == nullptr) {
            Logger::Error("Failed to resolve bind address '{}': {}", address, resolveResult);
            closesocket(m_listenSocket);
            m_listenSocket = INVALID_SOCKET;
            return false;
        }

        const auto* resolved = reinterpret_cast<sockaddr_in*>(result->ai_addr);
        serverAddr.sin_addr = resolved->sin_addr;
        freeaddrinfo(result);
    }
    
    if (bind(m_listenSocket, reinterpret_cast<sockaddr*>(&serverAddr), 
            sizeof(serverAddr)) == SOCKET_ERROR) {
        Logger::Error("Failed to bind to {}:{} - {}", address, port, WSAGetLastError());
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
        return false;
    }
    
    // 瀵偓婵娲冮崥?
    if (listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        Logger::Error("Failed to listen: {}", WSAGetLastError());
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
        return false;
    }
    
    m_running = true;
    
    // 閸氼垰濮╅幒銉ュ綀缁捐法鈻?
    m_acceptThread = std::thread(&TCPServer::AcceptThread, this);
    
    Logger::Info("TCP Server started on {}:{}", address, port);
    return true;
}

void TCPServer::Stop() {
    if (!m_running) {
        return;
    }
    
    Logger::Info("Stopping TCP Server...");
    
    m_running = false;
    
    // 閸忔娊妫撮惄鎴濇儔 socket
    if (m_listenSocket != INVALID_SOCKET) {
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
    }
    
    // 閺傤厼绱戦幍鈧張澶婎吂閹撮顏?
    m_connectionManager.DisconnectAll();
    
    // 缁涘绶熼幒銉ュ綀缁捐法鈻肩紒鎾存将
    if (m_acceptThread.joinable()) {
        m_acceptThread.join();
    }
    
    // 缁涘绶熼幍鈧張澶婎吂閹撮顏痪璺ㄢ柤缂佹挻娼?
    std::vector<std::thread> clientThreads;
    {
        std::lock_guard<std::mutex> lock(m_clientThreadsMutex);
        clientThreads.swap(m_clientThreads);
    }
    for (auto& thread : clientThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    Logger::Info("TCP Server stopped");
}

void TCPServer::SetMessageHandler(MessageCallback handler) {
    m_connectionManager.SetMessageCallback(handler);
}

void TCPServer::SetConnectionHandler(ConnectionCallback handler) {
    m_connectionManager.SetConnectionCallback(handler);
}

bool TCPServer::SendMessage(ClientId clientId, const std::string& message) {
    return m_connectionManager.SendMessage(clientId, message);
}

void TCPServer::BroadcastMessage(const std::string& message) {
    m_connectionManager.BroadcastMessage(message);
}

size_t TCPServer::GetClientCount() const {
    return m_connectionManager.GetClientCount();
}

void TCPServer::AcceptThread() {
    Logger::Debug("Accept thread started");
    
    while (m_running) {
        sockaddr_in clientAddr = {};
        int clientAddrLen = sizeof(clientAddr);
        
        SOCKET clientSocket = accept(m_listenSocket, 
                                     reinterpret_cast<sockaddr*>(&clientAddr),
                                     &clientAddrLen);
        
        if (clientSocket == INVALID_SOCKET) {
            if (m_running) {
                Logger::Error("Accept failed: {}", WSAGetLastError());
            }
            break;
        }
        
        // 閼惧嘲褰囩€广垺鍩涚粩顖氭勾閸р偓娣団剝浼?
        char addrStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, addrStr, sizeof(addrStr));
        uint16_t clientPort = ntohs(clientAddr.sin_port);
        
        // 濡偓閺屻儲娓舵径褑绻涢幒銉︽殶
        int maxConnections = ConfigManager::Instance().GetMaxConnections();
        if (maxConnections > 0 && 
            static_cast<int>(m_connectionManager.GetClientCount()) >= maxConnections) {
            Logger::Warning("Maximum connections reached, rejecting client from {}:{}", 
                          addrStr, clientPort);
            closesocket(clientSocket);
            continue;
        }
        
        // 濞ｈ濮炵€广垺鍩涚粩?
        ClientId clientId = m_connectionManager.AddClient(clientSocket, addrStr, clientPort);
        
        // 閸氼垰濮╃€广垺鍩涚粩顖涘复閺€鍓佸殠缁?
        {
            std::lock_guard<std::mutex> lock(m_clientThreadsMutex);
            m_clientThreads.emplace_back(&TCPServer::ClientReceiveThread, this, clientId);
        }
    }
    
    Logger::Debug("Accept thread stopped");
}

void TCPServer::ClientReceiveThread(ClientId clientId) {
    Logger::Debug("Client {} receive thread started", clientId);
    
    while (m_running) {
        if (!m_connectionManager.ProcessClientReceive(clientId)) {
            break;
        }
    }
    
    Logger::Debug("Client {} receive thread stopped", clientId);
}

void TCPServer::InitializeWSA() {
    std::lock_guard<std::mutex> lock(s_wsaMutex);
    
    if (s_wsaInitialized) {
        return;
    }
    
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        Logger::Error("WSAStartup failed: {}", result);
        return;
    }
    
    s_wsaInitialized = true;
    Logger::Debug("WSA initialized");
}

void TCPServer::CleanupWSA() {
    std::lock_guard<std::mutex> lock(s_wsaMutex);
    
    if (!s_wsaInitialized) {
        return;
    }
    
    WSACleanup();
    s_wsaInitialized = false;
    Logger::Debug("WSA cleaned up");
}

} // namespace MCP
