#pragma once
#include <string>
#include <shared_mutex>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace MCP {

/**
 * @brief 配置管理器（单例）
 */
class ConfigManager {
public:
    /**
     * @brief 获取单例实例
     */
    static ConfigManager& Instance();
    
    /**
     * @brief 加载配置文件
     * @param filePath 配置文件路径
     * @return 是否成功
     */
    bool Load(const std::string& filePath);
    
    /**
     * @brief 保存配置文件
     * @param filePath 配置文件路径
     * @return 是否成功
     */
    bool Save(const std::string& filePath);
    
    /**
     * @brief 获取配置项
     * @param key JSON 路径（如 "server.port"）
     * @param defaultValue 默认值
     * @return 配置值
     */
    template<typename T>
    T Get(const std::string& key, const T& defaultValue) const;
    
    /**
     * @brief 设置配置项
     * @param key JSON 路径
     * @param value 配置值
     */
    template<typename T>
    void Set(const std::string& key, const T& value);
    
    // 便捷方法
    std::string GetServerAddress() const;
    uint16_t GetServerPort() const;
    
    bool IsMemoryWriteAllowed() const;
    bool IsRegisterWriteAllowed() const;
    bool IsScriptExecutionAllowed() const;
    
    json GetOriginAllowlist() const;
    json GetHostAllowlist() const;
    bool IsHttpAuthenticationEnabled() const;
    std::string GetHttpAuthenticationToken() const;
    std::string GetLogLevel() const;
    std::string GetLogFile() const;
    bool IsLoggingEnabled() const;
    
    uint32_t GetRequestTimeout() const;
    uint32_t GetStepTimeout() const;
    
    /**
     * @brief 获取配置文件路径
     * @return 配置文件路径
     */
    std::string GetConfigPath() const;
    
    /**
     * @brief 获取默认配置
     * @return 默认配置的 JSON 对象
     */
    json GetDefaultConfig() const;

private:
    ConfigManager() = default;
    ~ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    
    json GetNestedValue(const std::string& key) const;
    void SetNestedValue(const std::string& key, const json& value);
    json CreateDefaultConfig() const;
    bool CreateConfigFile(const std::string& filePath);
    
    json m_config;
    mutable std::shared_mutex m_mutex;
    std::string m_configPath;  // 存储配置文件路径
};

// 模板函数实现
template<typename T>
T ConfigManager::Get(const std::string& key, const T& defaultValue) const {
    std::shared_lock lock(m_mutex);
    
    try {
        json value = GetNestedValue(key);
        if (value.is_null()) {
            return defaultValue;
        }
        return value.get<T>();
    } catch (...) {
        return defaultValue;
    }
}

template<typename T>
void ConfigManager::Set(const std::string& key, const T& value) {
    std::unique_lock lock(m_mutex);
    SetNestedValue(key, json(value));
}

} // namespace MCP
