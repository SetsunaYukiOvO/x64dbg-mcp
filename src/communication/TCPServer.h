#pragma once
#include "ConnectionManager.h"
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <vector>
#include <mutex>

namespace MCP {

/**
 * @brief TCP 服务器
 */
class TCPServer {
public:
    TCPServer();
    ~TCPServer();
    
    /**
     * @brief 启动服务器
     * @param address 监听地址
     * @param port 监听端口
     * @return 是否成功
     */
    bool Start(const std::string& address, uint16_t port);
    
    /**
     * @brief 停止服务器
     */
    void Stop();
    
    /**
     * @brief 服务器是否运行中
     */
    bool IsRunning() const { return m_running; }
    
    /**
     * @brief 设置消息处理回调
     */
    void SetMessageHandler(MessageCallback handler);
    
    /**
     * @brief 设置连接事件回调
     */
    void SetConnectionHandler(ConnectionCallback handler);
    
    /**
     * @brief 发送消息给客户端
     */
    bool SendMessage(ClientId clientId, const std::string& message);
    
    /**
     * @brief 广播消息
     */
    void BroadcastMessage(const std::string& message);
    
    /**
     * @brief 获取连接的客户端数量
     */
    size_t GetClientCount() const;

private:
    void AcceptThread();
    void ClientReceiveThread(ClientId clientId);
    
    SOCKET m_listenSocket;
    std::atomic<bool> m_running;
    
    std::thread m_acceptThread;
    std::vector<std::thread> m_clientThreads;
    std::mutex m_clientThreadsMutex;
    
    ConnectionManager m_connectionManager;
    
    std::string m_address;
    uint16_t m_port;
    
    static bool s_wsaInitialized;
    static std::mutex s_wsaMutex;
    static void InitializeWSA();
    static void CleanupWSA();
};

} // namespace MCP
