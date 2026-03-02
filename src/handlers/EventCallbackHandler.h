#pragma once
#include <nlohmann/json.hpp>
#include <functional>
#include <map>
#include <mutex>
#include <string>

namespace MCP {

/**
 * @brief 事件类型
 */
enum class EventType {
    Breakpoint,      // 断点命中
    Exception,       // 异常
    ProcessCreated,  // 进程创建
    ProcessExited,   // 进程退出
    ThreadCreated,   // 线程创建
    ThreadExited,    // 线程退出
    ModuleLoaded,    // 模块加载
    ModuleUnloaded,  // 模块卸载
    DebugEvent       // 其他调试事件
};

/**
 * @brief 事件信息基类
 */
struct EventInfo {
    EventType type;
    uint64_t timestamp;
    
    virtual ~EventInfo() = default;
    virtual nlohmann::json ToJson() const = 0;
};

/**
 * @brief 断点事件
 */
struct BreakpointEvent : public EventInfo {
    uint64_t address;
    std::string name;
    uint32_t hitCount;
    
    nlohmann::json ToJson() const override;
};

/**
 * @brief 异常事件
 */
struct ExceptionEvent : public EventInfo {
    uint32_t exceptionCode;
    uint64_t exceptionAddress;
    std::string exceptionName;
    bool firstChance;
    
    nlohmann::json ToJson() const override;
};

/**
 * @brief 模块事件
 */
struct ModuleEvent : public EventInfo {
    std::string moduleName;
    std::string modulePath;
    uint64_t base;
    uint64_t size;
    
    nlohmann::json ToJson() const override;
};

/**
 * @brief 事件回调处理器
 * 处理 x64dbg 的各种调试事件并通过 JSON-RPC 通知客户端
 */
class EventCallbackHandler {
public:
    /**
     * @brief 获取单例实例
     */
    static EventCallbackHandler& Instance();
    
    /**
     * @brief 初始化事件回调
     */
    void Initialize();
    
    /**
     * @brief 清理事件回调
     */
    void Cleanup();
    
    /**
     * @brief 启用/禁用事件通知
     * @param enabled 是否启用
     */
    void SetEventsEnabled(bool enabled);
    
    /**
     * @brief 设置事件过滤器
     * @param eventType 事件类型
     * @param enabled 是否启用该类型事件
     */
    void SetEventFilter(EventType eventType, bool enabled);
    
    // x64dbg 回调函数
    static void OnBreakpoint(uint64_t address);
    static void OnException(uint32_t code, uint64_t address);
    static void OnModuleLoad(const char* name, uint64_t base, uint64_t size);
    static void OnModuleUnload(const char* name);
    static void OnCreateProcess();
    static void OnExitProcess();
    
private:
    EventCallbackHandler() = default;
    ~EventCallbackHandler() = default;
    EventCallbackHandler(const EventCallbackHandler&) = delete;
    EventCallbackHandler& operator=(const EventCallbackHandler&) = delete;
    
    void BroadcastEvent(const EventInfo& event);
    std::string EventTypeToString(EventType type);
    
    bool m_eventsEnabled = true;
    std::map<EventType, bool> m_eventFilters;
    mutable std::mutex m_mutex;
};

} // namespace MCP
