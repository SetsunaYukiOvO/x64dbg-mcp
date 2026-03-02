#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <memory>
#include <functional>

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

namespace MCP {

// 客户端 ID 类型
using ClientId = uint64_t;

/**
 * @brief 客户端连接上下文
 */
struct ClientContext {
    ClientId id;
    SOCKET socket;
    std::string address;
    uint16_t port;
    std::vector<uint8_t> receiveBuffer;
    std::atomic<bool> connected;
    
    ClientContext(ClientId cid, SOCKET sock, const std::string& addr, uint16_t p)
        : id(cid), socket(sock), address(addr), port(p), connected(true) {}
};

/**
 * @brief 消息回调函数类型
 */
using MessageCallback = std::function<void(ClientId, const std::string&)>;

/**
 * @brief 连接事件回调函数类型
 */
using ConnectionCallback = std::function<void(ClientId, bool connected)>;

/**
 * @brief 连接管理器
 */
class ConnectionManager {
public:
    ConnectionManager();
    ~ConnectionManager();
    
    /**
     * @brief 设置消息回调
     */
    void SetMessageCallback(MessageCallback callback);
    
    /**
     * @brief 设置连接事件回调
     */
    void SetConnectionCallback(ConnectionCallback callback);
    
    /**
     * @brief 添加客户端连接
     * @param socket 客户端 socket
     * @param address 客户端地址
     * @param port 客户端端口
     * @return 客户端 ID
     */
    ClientId AddClient(SOCKET socket, const std::string& address, uint16_t port);
    
    /**
     * @brief 移除客户端连接
     * @param clientId 客户端 ID
     */
    void RemoveClient(ClientId clientId);
    
    /**
     * @brief 发送消息给客户端
     * @param clientId 客户端 ID
     * @param message 消息内容
     * @return 是否成功
     */
    bool SendMessage(ClientId clientId, const std::string& message);
    
    /**
     * @brief 广播消息给所有客户端
     * @param message 消息内容
     */
    void BroadcastMessage(const std::string& message);
    
    /**
     * @brief 处理客户端接收（在接收线程中调用）
     * @param clientId 客户端 ID
     */
    bool ProcessClientReceive(ClientId clientId);
    
    /**
     * @brief 获取客户端数量
     */
    size_t GetClientCount() const;
    
    /**
     * @brief 获取所有客户端 ID
     */
    std::vector<ClientId> GetClientIds() const;
    
    /**
     * @brief 关闭所有连接
     */
    void DisconnectAll();

private:
    std::shared_ptr<ClientContext> GetClient(ClientId clientId);
    void NotifyConnection(ClientId clientId, bool connected);
    
    std::unordered_map<ClientId, std::shared_ptr<ClientContext>> m_clients;
    mutable std::mutex m_mutex;
    
    MessageCallback m_messageCallback;
    ConnectionCallback m_connectionCallback;
    
    std::atomic<ClientId> m_nextClientId;
};

} // namespace MCP
