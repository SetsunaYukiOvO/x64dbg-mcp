#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <sstream>
#include <chrono>
#include <iomanip>

namespace MCP {

/**
 * @brief 日志级别
 */
enum class LogLevel {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warning = 3,
    Error = 4,
    Critical = 5
};

/**
 * @brief 简单的日志系统
 */
class Logger {
public:
    /**
     * @brief 初始化日志系统
     * @param filePath 日志文件路径
     * @param level 最小日志级别
     * @param consoleOutput 是否输出到控制台
     */
    static bool Initialize(const std::string& filePath, 
                          LogLevel level = LogLevel::Info,
                          bool consoleOutput = true);
    
    /**
     * @brief 关闭日志系统
     */
    static void Shutdown();
    
    /**
     * @brief 设置日志级别
     */
    static void SetLevel(LogLevel level);
    
    /**
     * @brief 获取日志级别
     */
    static LogLevel GetLevel();
    
    /**
     * @brief 记录日志
     */
    static void Log(LogLevel level, const std::string& message);
    
    /**
     * @brief Trace 级别日志
     */
    template<typename... Args>
    static void Trace(const std::string& format, Args&&... args) {
        LogFormatted(LogLevel::Trace, format, std::forward<Args>(args)...);
    }
    
    /**
     * @brief Debug 级别日志
     */
    template<typename... Args>
    static void Debug(const std::string& format, Args&&... args) {
        LogFormatted(LogLevel::Debug, format, std::forward<Args>(args)...);
    }
    
    /**
     * @brief Info 级别日志
     */
    template<typename... Args>
    static void Info(const std::string& format, Args&&... args) {
        LogFormatted(LogLevel::Info, format, std::forward<Args>(args)...);
    }
    
    /**
     * @brief Warning 级别日志
     */
    template<typename... Args>
    static void Warning(const std::string& format, Args&&... args) {
        LogFormatted(LogLevel::Warning, format, std::forward<Args>(args)...);
    }
    
    /**
     * @brief Error 级别日志
     */
    template<typename... Args>
    static void Error(const std::string& format, Args&&... args) {
        LogFormatted(LogLevel::Error, format, std::forward<Args>(args)...);
    }
    
    /**
     * @brief Critical 级别日志
     */
    template<typename... Args>
    static void Critical(const std::string& format, Args&&... args) {
        LogFormatted(LogLevel::Critical, format, std::forward<Args>(args)...);
    }

private:
    static std::string GetTimestamp();
    static std::string LevelToString(LogLevel level);
    
    template<typename... Args>
    static void LogFormatted(LogLevel level, const std::string& format, Args&&... args);
    
    static std::string FormatLogMessage(const std::string& format);
    
    template<typename T, typename... Args>
    static std::string FormatLogMessage(const std::string& format, T&& value, Args&&... args);

    template<typename T>
    static void AppendFormattedValue(std::ostringstream& oss, T&& value);
    static void AppendFormattedValue(std::ostringstream& oss, const char* value);
    static void AppendFormattedValue(std::ostringstream& oss, char* value);
    
    static std::ofstream m_file;
    static LogLevel m_level;
    static bool m_consoleOutput;
    static std::mutex m_mutex;
    static bool m_initialized;
};

// 模板函数实现
template<typename... Args>
void Logger::LogFormatted(LogLevel level, const std::string& format, Args&&... args) {
    std::string message = FormatLogMessage(format, std::forward<Args>(args)...);
    Log(level, message);
}

template<typename T, typename... Args>
std::string Logger::FormatLogMessage(const std::string& format, T&& value, Args&&... args) {
    std::ostringstream oss;
    size_t pos = format.find("{}");
    
    if (pos != std::string::npos) {
        oss << format.substr(0, pos);
        AppendFormattedValue(oss, std::forward<T>(value));
        std::string remaining = format.substr(pos + 2);
        oss << FormatLogMessage(remaining, std::forward<Args>(args)...);
    } else {
        oss << format;
    }
    
    return oss.str();
}

template<typename T>
void Logger::AppendFormattedValue(std::ostringstream& oss, T&& value) {
    oss << std::forward<T>(value);
}

inline void Logger::AppendFormattedValue(std::ostringstream& oss, const char* value) {
    oss << (value ? value : "(null)");
}

inline void Logger::AppendFormattedValue(std::ostringstream& oss, char* value) {
    oss << (value ? value : "(null)");
}

} // namespace MCP

// 日志宏定义，方便使用
#ifdef ENABLE_LOGGING
    #define LOG_TRACE(...)    ::MCP::Logger::Trace(__VA_ARGS__)
    #define LOG_DEBUG(...)    ::MCP::Logger::Debug(__VA_ARGS__)
    #define LOG_INFO(...)     ::MCP::Logger::Info(__VA_ARGS__)
    #define LOG_WARNING(...)  ::MCP::Logger::Warning(__VA_ARGS__)
    #define LOG_ERROR(...)    ::MCP::Logger::Error(__VA_ARGS__)
    #define LOG_CRITICAL(...) ::MCP::Logger::Critical(__VA_ARGS__)
#else
    #define LOG_TRACE(...)    ((void)0)
    #define LOG_DEBUG(...)    ((void)0)
    #define LOG_INFO(...)     ((void)0)
    #define LOG_WARNING(...)  ((void)0)
    #define LOG_ERROR(...)    ((void)0)
    #define LOG_CRITICAL(...) ((void)0)
#endif
