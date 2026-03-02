#include "Logger.h"
#include <iostream>
#include <ctime>

namespace MCP {

// 静态成员初始化
std::ofstream Logger::m_file;
LogLevel Logger::m_level = LogLevel::Info;
bool Logger::m_consoleOutput = true;
std::mutex Logger::m_mutex;
bool Logger::m_initialized = false;

bool Logger::Initialize(const std::string& filePath, LogLevel level, bool consoleOutput) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_initialized) {
        return true;
    }
    
    m_file.open(filePath, std::ios::out | std::ios::app);
    if (!m_file.is_open()) {
        return false;
    }
    
    m_level = level;
    m_consoleOutput = consoleOutput;
    m_initialized = true;
    
    // ✅ 修复：不在持有锁的情况下调用Log，避免递归锁定
    // Log(LogLevel::Info, "Logger initialized");
    return true;
}

void Logger::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_initialized) {
        return;
    }
    
    // ✅ 修复：不在持有锁的情况下调用Log
    // Log(LogLevel::Info, "Logger shutting down");
    
    if (m_file.is_open()) {
        m_file.close();
    }
    
    m_initialized = false;
}

void Logger::SetLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_level = level;
}

LogLevel Logger::GetLevel() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_level;
}

void Logger::Log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized || level < m_level) {
        return;
    }
    
    std::string timestamp = GetTimestamp();
    std::string levelStr = LevelToString(level);
    std::string logLine = "[" + timestamp + "] [" + levelStr + "] " + message;
    
    // 写入文件
    if (m_file.is_open()) {
        m_file << logLine << std::endl;
        m_file.flush();
    }
    
    // 输出到控制台
    if (m_consoleOutput) {
        if (level >= LogLevel::Error) {
            std::cerr << logLine << std::endl;
        } else {
            std::cout << logLine << std::endl;
        }
    }
}

std::string Logger::GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm timeInfo;
    localtime_s(&timeInfo, &time);
    
    std::ostringstream oss;
    oss << std::put_time(&timeInfo, "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    return oss.str();
}

std::string Logger::LevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Trace:    return "TRACE";
        case LogLevel::Debug:    return "DEBUG";
        case LogLevel::Info:     return "INFO ";
        case LogLevel::Warning:  return "WARN ";
        case LogLevel::Error:    return "ERROR";
        case LogLevel::Critical: return "CRIT ";
        default:                 return "?????";
    }
}

std::string Logger::FormatLogMessage(const std::string& format) {
    return format;
}

} // namespace MCP
