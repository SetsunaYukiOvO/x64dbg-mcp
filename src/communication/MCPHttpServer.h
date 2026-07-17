/**
 * @file MCPHttpServer.h
 * @brief MCP HTTP Server with SSE (Server-Sent Events) support
 * 
 * 实现 MCP 协议的 HTTP 服务器，使用 SSE 作为传输层。
 * VSCode/Claude Desktop 可以直接通过 HTTP 连接此服务器。
 */

#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <future>
#include <memory>
#include <functional>
#include <vector>
#include <mutex>
#include <unordered_set>
#include <winsock2.h>
#include <nlohmann/json_fwd.hpp>

namespace MCP {

/**
 * @brief MCP HTTP Server 实现
 * 
 * 提供 HTTP 接口：
 * - GET /sse - SSE 连接，接收来自 MCP 客户端的消息
 * - POST /message - 发送消息到 MCP 客户端
 */
class MCPHttpServer {
public:
    MCPHttpServer();
    ~MCPHttpServer();

    // 启动服务器
    bool Start(const std::string& host = "127.0.0.1", int port = 3000);
    
    // 停止服务器
    void Stop();
    
    // 是否正在运行
    bool IsRunning() const { return m_running; }
    
    // 获取服务器地址
    std::string GetAddress() const { return m_host + ":" + std::to_string(m_port); }
    static bool BroadcastNotification(const std::string& method, const nlohmann::json& params);

private:
    struct MCPToolCallResult {
        bool success = false;
        std::string contentText;
        int errorCode = -32603;
        std::string errorMessage;
    };

    // 服务器主循环
    void ServerLoop();
    
    // 处理客户端连接
    void HandleClient(SOCKET clientSocket);
    
    // 处理 HTTP 请求
    void HandleHttpRequest(SOCKET clientSocket, const std::string& request);
    
    // 处理 SSE 连接
    void HandleSSE(SOCKET clientSocket, const std::string& origin);

    // 处理 POST 消息
    void HandlePostMessage(SOCKET clientSocket, const std::string& body,
                           const std::string& origin);

    // Streamable HTTP transport (MCP 2025-03-26) — single endpoint /mcp.
    // POST returns the JSON-RPC response inline as application/json.
    // GET opens a long-lived SSE stream used only for server-to-client pushes
    // (no endpoint event handshake, unlike the legacy /sse transport).
    void HandleStreamableHttpPost(SOCKET clientSocket, const std::string& body,
                                  const std::string& origin);
    void HandleStreamableHttpStream(SOCKET clientSocket, const std::string& origin);
    
    // 处理 MCP 方法调用
    std::string HandleMCPMethod(const std::string& method, const std::string& requestId, const std::string& body);
    
    // 解析 JSON-RPC 请求
    bool ParseJsonRpcRequest(const std::string& json, std::string& method, std::string& requestId, bool& hasRequestId);
    
    // 调用 MCP 工具
    MCPToolCallResult CallMCPTool(const std::string& toolName, const nlohmann::json& arguments);
    
    // Parse HTTP request
    bool ParseHttpRequest(const std::string& request, std::string& method,
                         std::string& path, std::string& body);

    // Validate Origin + Host headers to prevent CSRF/DNS-rebinding attacks
    // - If Origin is present, it must be in the configured allowlist (empty by default).
    // If Host is present, it must match a loopback address, the bind address, or the configured allowlist.
    bool ValidateCrossOriginAndHost(const std::string& origin, const std::string& host);
    
    // 发送 HTTP 响应
    void SendHttpResponse(SOCKET socket, int statusCode, const std::string& body,
                         const std::string& contentType = "application/json",
                         const std::string& origin = "");
    
    // 发送 SSE 事件
    bool SendSSEEvent(SOCKET socket, const std::string& event, const std::string& data);
    bool BroadcastSSEEvent(const std::string& event, const std::string& data);
    void CleanupFinishedClientTasks();

    std::string m_host;
    int m_port;
    SOCKET m_listenSocket;
    std::atomic<bool> m_running;
    std::thread m_serverThread;
    std::vector<std::future<void>> m_clientTasks;
    std::mutex m_clientTasksMutex;
    std::unordered_set<SOCKET> m_activeClientSockets;
    std::mutex m_activeClientSocketsMutex;
    std::unordered_set<SOCKET> m_sseClientSockets;
    std::mutex m_sseClientSocketsMutex;
    std::mutex m_sseSendMutex;
    std::atomic<int> m_requestId;

    static std::mutex s_activeInstanceMutex;
    static MCPHttpServer* s_activeInstance;
};

} // namespace MCP
