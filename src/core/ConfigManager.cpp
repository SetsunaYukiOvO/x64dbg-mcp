#include "ConfigManager.h"
#include "Logger.h"
#include <fstream>
#include <sstream>
#include <filesystem>

namespace MCP {

ConfigManager& ConfigManager::Instance() {
    static ConfigManager instance;
    return instance;
}

json ConfigManager::CreateDefaultConfig() const {
    json config;
    
    config["version"] = "1.0.6";
    
    // Server configuration
    config["server"]["address"] = "127.0.0.1";
    config["server"]["port"] = 3000;
    
    // Permissions
    config["permissions"]["allow_memory_write"] = true;
    config["permissions"]["allow_register_write"] = true;
    config["permissions"]["allow_script_execution"] = true;
    config["permissions"]["allow_breakpoint_modification"] = true;
    config["permissions"]["allowed_methods"] = json::array({
        "debug.*", "register.*", "memory.*", "breakpoint.*",
        "disasm.*", "disassembly.*", "module.*", "symbol.*",
        "thread.*", "stack.*", "comment.*", "script.*",
        "context.*", "dump.*", "eval.*", "xref.*",
        "function.*", "assembler.*", "bookmark.*", "patch.*"
    });
    
    // Logging
    config["logging"]["enabled"] = true;
    config["logging"]["level"] = "info";
    config["logging"]["file"] = "x64dbg_mcp.log";
    config["logging"]["max_file_size_mb"] = 10;
    config["logging"]["console_output"] = true;
    
    // Timeout
    config["timeout"]["request_timeout_ms"] = 30000;
    config["timeout"]["step_timeout_ms"] = 10000;
    config["timeout"]["memory_read_timeout_ms"] = 5000;
    
    // Features
    config["features"]["enable_notifications"] = true;
    config["features"]["enable_heartbeat"] = true;
    config["features"]["heartbeat_interval_seconds"] = 30;
    config["features"]["enable_batch_requests"] = true;
    config["features"]["auto_start_mcp_on_plugin_load"] = false;
    
    return config;
}

bool ConfigManager::CreateConfigFile(const std::string& filePath) {
    try {
        // 创建目录（如果不存在）
        std::filesystem::path path(filePath);
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }
        
        // 创建默认配置
        json defaultConfig = CreateDefaultConfig();
        
        // 写入文件
        std::ofstream file(filePath);
        if (!file.is_open()) {
            Logger::Error("Failed to create config file: {}", filePath);
            return false;
        }
        
        file << defaultConfig.dump(2);  // 使用2空格缩进
        file.close();
        
        Logger::Info("Created default config file: {}", filePath);
        return true;
    } catch (const std::exception& e) {
        Logger::Error("Failed to create config file: {}", e.what());
        return false;
    }
}

bool ConfigManager::Load(const std::string& filePath) {
    std::unique_lock lock(m_mutex);
    
    // 存储配置文件路径
    m_configPath = filePath;
    
    // 检查文件是否存在
    if (!std::filesystem::exists(filePath)) {
        lock.unlock();
        Logger::Warning("Config file not found: {}, creating default config...", filePath);
        
        // 创建默认配置文件
        if (!CreateConfigFile(filePath)) {
            return false;
        }
        
        // 重新获取锁并加载默认配置
        lock.lock();
        m_config = CreateDefaultConfig();
        lock.unlock();
        Logger::Info("Default configuration loaded");
        return true;
    }
    
    std::ifstream file(filePath);
    if (!file.is_open()) {
        // ✅ 修复：先释放锁再调用Logger，避免潜在的死锁
        lock.unlock();
        Logger::Error("Failed to open config file: {}", filePath);
        return false;
    }
    
    try {
        json loadedConfig;
        file >> loadedConfig;
        m_config = std::move(loadedConfig);
        lock.unlock();
        Logger::Info("Configuration loaded from: {}", filePath);
        return true;
    } catch (const json::exception& e) {
        lock.unlock();
        Logger::Error("Failed to parse config file: {}", e.what());
        return false;
    }
}

bool ConfigManager::Save(const std::string& filePath) {
    std::shared_lock lock(m_mutex);
    
    std::ofstream file(filePath);
    if (!file.is_open()) {
        // ✅ 修复：先释放锁再调用Logger
        lock.unlock();
        Logger::Error("Failed to open config file for writing: {}", filePath);
        return false;
    }
    
    try {
        file << m_config.dump(2);
        lock.unlock();
        Logger::Info("Configuration saved to: {}", filePath);
        return true;
    } catch (const json::exception& e) {
        lock.unlock();
        Logger::Error("Failed to write config file: {}", e.what());
        return false;
    }
}

json ConfigManager::GetNestedValue(const std::string& key) const {
    std::istringstream iss(key);
    std::string token;
    json current = m_config;
    
    while (std::getline(iss, token, '.')) {
        if (current.contains(token)) {
            current = current[token];
        } else {
            return json();
        }
    }
    
    return current;
}

void ConfigManager::SetNestedValue(const std::string& key, const json& value) {
    std::istringstream iss(key);
    std::string token;
    std::vector<std::string> keys;
    
    while (std::getline(iss, token, '.')) {
        keys.push_back(token);
    }
    
    if (keys.empty()) {
        return;
    }
    
    json* current = &m_config;
    for (size_t i = 0; i < keys.size() - 1; ++i) {
        if (!current->contains(keys[i]) || !(*current)[keys[i]].is_object()) {
            (*current)[keys[i]] = json::object();
        }
        current = &(*current)[keys[i]];
    }
    
    (*current)[keys.back()] = value;
}

std::string ConfigManager::GetServerAddress() const {
    std::string address = Get<std::string>("server.address", "127.0.0.1");
    if (address.empty()) {
        Logger::Warning("Empty server.address value, fallback to 127.0.0.1");
        return "127.0.0.1";
    }
    return address;
}

uint16_t ConfigManager::GetServerPort() const {
    int port = Get<int>("server.port", 3000);
    if (port < 1 || port > 65535) {
        Logger::Warning("Invalid server.port value: {}, fallback to 3000", port);
        return 3000;
    }
    return static_cast<uint16_t>(port);
}

bool ConfigManager::IsMemoryWriteAllowed() const {
    return Get<bool>("permissions.allow_memory_write", false);
}

bool ConfigManager::IsRegisterWriteAllowed() const {
    return Get<bool>("permissions.allow_register_write", false);
}

bool ConfigManager::IsScriptExecutionAllowed() const {
    return Get<bool>("permissions.allow_script_execution", false);
}

std::string ConfigManager::GetLogLevel() const {
    return Get<std::string>("logging.level", "info");
}

std::string ConfigManager::GetLogFile() const {
    return Get<std::string>("logging.file", "x64dbg_mcp.log");
}

bool ConfigManager::IsLoggingEnabled() const {
    return Get<bool>("logging.enabled", true);
}

uint32_t ConfigManager::GetRequestTimeout() const {
    return static_cast<uint32_t>(Get<int>("timeout.request_timeout_ms", 30000));
}

uint32_t ConfigManager::GetStepTimeout() const {
    return static_cast<uint32_t>(Get<int>("timeout.step_timeout_ms", 10000));
}

std::string ConfigManager::GetConfigPath() const {
    std::shared_lock lock(m_mutex);
    return m_configPath;
}

json ConfigManager::GetDefaultConfig() const {
    return CreateDefaultConfig();
}

} // namespace MCP
