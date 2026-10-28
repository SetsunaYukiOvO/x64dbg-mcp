#include "Logger.h"
#include <iostream>
#include <ctime>
#include <filesystem>
#include <algorithm>

namespace MCP {

// 静态成员初始化
std::ofstream Logger::m_file;
std::string Logger::m_filePath;
uint64_t Logger::m_fileSize = 0;
uint64_t Logger::m_maxFileSize = 10 * 1024 * 1024;
LogLevel Logger::m_level = LogLevel::Info;
bool Logger::m_consoleOutput = true;
std::mutex Logger::m_mutex;
bool Logger::m_initialized = false;

bool Logger::Initialize(const std::string& filePath, LogLevel level, bool consoleOutput, uint64_t maxFileSizeBytes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_initialized) {
        return true;
    }
    
    const auto path = std::filesystem::u8path(filePath);
    std::error_code error;
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), error);
    m_file.open(path, std::ios::out | std::ios::app | std::ios::binary);
    if (!m_file.is_open()) {
        return false;
    }
    
    m_filePath = filePath;
    m_maxFileSize = std::max<uint64_t>(maxFileSizeBytes, 128);
    m_fileSize = std::filesystem::file_size(path, error);
    if (error) m_fileSize = 0;
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
        if (logLine.size() + 1 > m_maxFileSize) logLine.resize(static_cast<size_t>(m_maxFileSize - 1));
        if (m_fileSize > m_maxFileSize - logLine.size() - 1) {
            m_file.close();
            const auto path = std::filesystem::u8path(m_filePath);
            const auto backup = std::filesystem::u8path(m_filePath + ".1");
            std::error_code error;
            std::filesystem::remove(backup, error);
            std::filesystem::rename(path, backup, error);
            m_file.open(path, std::ios::out | std::ios::trunc | std::ios::binary);
            m_fileSize = 0;
        }
        m_file << logLine << '\n';
        m_file.flush();
        m_fileSize += logLine.size() + 1;
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
