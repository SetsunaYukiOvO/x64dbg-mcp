#include "ConnectionManager.h"
#include "MessageTransport.h"
#include "../core/Logger.h"

namespace MCP {

ConnectionManager::ConnectionManager()
    : m_nextClientId(1)
{
}

ConnectionManager::~ConnectionManager() {
    DisconnectAll();
}

void ConnectionManager::SetMessageCallback(MessageCallback callback) {
    m_messageCallback = callback;
}

void ConnectionManager::SetConnectionCallback(ConnectionCallback callback) {
    m_connectionCallback = callback;
}

ClientId ConnectionManager::AddClient(SOCKET socket, const std::string& address, uint16_t port) {
    ClientId id = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        id = m_nextClientId++;
        auto client = std::make_shared<ClientContext>(id, socket, address, port);
        m_clients[id] = client;
    }
    
    Logger::Info("Client {} connected from {}:{}", id, address, port);
    
    NotifyConnection(id, true);
    return id;
}

void ConnectionManager::RemoveClient(ClientId clientId) {
    std::shared_ptr<ClientContext> client;
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_clients.find(clientId);
        if (it == m_clients.end()) {
            return;
        }
        client = it->second;
        m_clients.erase(it);
    }
    
    client->connected = false;
    closesocket(client->socket);
    
    Logger::Info("Client {} disconnected", clientId);
    NotifyConnection(clientId, false);
}

bool ConnectionManager::SendMessage(ClientId clientId, const std::string& message) {
    auto client = GetClient(clientId);
    if (!client || !client->connected) {
        Logger::Warning("Cannot send to disconnected client {}", clientId);
        return false;
    }
    
    try {
        // 编码消息
        auto encoded = MessageTransport::Encode(message);
        
        // 发送数据
        int totalSent = 0;
        while (totalSent < static_cast<int>(encoded.size())) {
            int sent = send(client->socket, 
                          reinterpret_cast<const char*>(encoded.data() + totalSent),
                          static_cast<int>(encoded.size() - totalSent), 
                          0);
            
            if (sent == SOCKET_ERROR) {
                Logger::Error("Send failed for client {}: {}", clientId, WSAGetLastError());
                RemoveClient(clientId);
                return false;
            }
            
            totalSent += sent;
        }
        
        Logger::Trace("Sent {} bytes to client {}", encoded.size(), clientId);
        return true;
        
    } catch (const std::exception& ex) {
        Logger::Error("Exception sending to client {}: {}", clientId, ex.what());
        return false;
    }
}

void ConnectionManager::BroadcastMessage(const std::string& message) {
    auto clientIds = GetClientIds();
    
    Logger::Debug("Broadcasting message to {} clients", clientIds.size());
    
    for (ClientId id : clientIds) {
        SendMessage(id, message);
    }
}

bool ConnectionManager::ProcessClientReceive(ClientId clientId) {
    auto client = GetClient(clientId);
    if (!client || !client->connected) {
        return false;
    }
    
    uint8_t buffer[4096];
    int received = recv(client->socket, reinterpret_cast<char*>(buffer), sizeof(buffer), 0);
    
    if (received == SOCKET_ERROR || received == 0) {
        if (received == SOCKET_ERROR) {
            Logger::Debug("Recv error for client {}: {}", clientId, WSAGetLastError());
        }
        RemoveClient(clientId);
        return false;
    }
    
    // 添加到接收缓冲区
    client->receiveBuffer.insert(client->receiveBuffer.end(), buffer, buffer + received);
    
    // 尝试解码消息
    while (true) {
        std::string message;
        size_t bytesConsumed = 0;
        
        bool hasMessage = MessageTransport::Decode(
            client->receiveBuffer.data(),
            client->receiveBuffer.size(),
            message,
            bytesConsumed
        );
        
        if (!hasMessage) {
            break; // 没有完整消息
        }
        
        // 移除已处理的数据
        client->receiveBuffer.erase(
            client->receiveBuffer.begin(),
            client->receiveBuffer.begin() + bytesConsumed
        );
        
        // 调用消息回调
        if (m_messageCallback) {
            try {
                m_messageCallback(clientId, message);
            } catch (const std::exception& ex) {
                Logger::Error("Exception in message callback: {}", ex.what());
            }
        }
    }
    return GetClient(clientId) != nullptr;
}

size_t ConnectionManager::GetClientCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_clients.size();
}

std::vector<ClientId> ConnectionManager::GetClientIds() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<ClientId> ids;
    ids.reserve(m_clients.size());
    
    for (const auto& pair : m_clients) {
        ids.push_back(pair.first);
    }
    
    return ids;
}

void ConnectionManager::DisconnectAll() {
    auto clientIds = GetClientIds();
    
    Logger::Info("Disconnecting {} clients", clientIds.size());
    
    for (ClientId id : clientIds) {
        RemoveClient(id);
    }
}

std::shared_ptr<ClientContext> ConnectionManager::GetClient(ClientId clientId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_clients.find(clientId);
    return (it != m_clients.end()) ? it->second : nullptr;
}

void ConnectionManager::NotifyConnection(ClientId clientId, bool connected) {
    if (m_connectionCallback) {
        try {
            m_connectionCallback(clientId, connected);
        } catch (const std::exception& ex) {
            Logger::Error("Exception in connection callback: {}", ex.what());
        }
    }
}

} // namespace MCP
