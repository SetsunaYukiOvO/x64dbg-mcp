#include "EventCallbackHandler.h"
#include "../core/Logger.h"
#include "../utils/StringUtils.h"
#include <chrono>
#include <sstream>
#include <iomanip>

namespace MCP {

EventCallbackHandler& EventCallbackHandler::Instance() {
    static EventCallbackHandler instance;
    return instance;
}

void EventCallbackHandler::Initialize() {
    Logger::Info("Initializing event callback handler");
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 默认启用所有事件
    m_eventsEnabled = true;
    m_eventFilters[EventType::Breakpoint] = true;
    m_eventFilters[EventType::Exception] = true;
    m_eventFilters[EventType::ProcessCreated] = true;
    m_eventFilters[EventType::ProcessExited] = true;
    m_eventFilters[EventType::ModuleLoaded] = true;
    m_eventFilters[EventType::ModuleUnloaded] = true;
}

void EventCallbackHandler::Cleanup() {
    Logger::Info("Cleaning up event callback handler");
    std::lock_guard<std::mutex> lock(m_mutex);
    m_eventsEnabled = false;
}

void EventCallbackHandler::SetEventsEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_eventsEnabled = enabled;
    Logger::Info("Events {}", enabled ? "enabled" : "disabled");
}

void EventCallbackHandler::SetEventFilter(EventType eventType, bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_eventFilters[eventType] = enabled;
    Logger::Debug("Event filter for {} set to {}", 
                  EventTypeToString(eventType), 
                  enabled ? "enabled" : "disabled");
}

void EventCallbackHandler::OnBreakpoint(uint64_t address) {
    auto& instance = Instance();

    bool shouldBroadcast = false;
    {
        std::lock_guard<std::mutex> lock(instance.m_mutex);
        const auto it = instance.m_eventFilters.find(EventType::Breakpoint);
        shouldBroadcast = instance.m_eventsEnabled &&
                         it != instance.m_eventFilters.end() && it->second;
    }
    if (!shouldBroadcast) {
        return;
    }
    
    BreakpointEvent event;
    event.type = EventType::Breakpoint;
    event.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    event.address = address;
    event.hitCount = 0;  // 从断点管理器获取
    
    instance.BroadcastEvent(event);
    Logger::Debug("Breakpoint hit at 0x{:X}", address);
}

void EventCallbackHandler::OnException(uint32_t code, uint64_t address) {
    auto& instance = Instance();

    bool shouldBroadcast = false;
    {
        std::lock_guard<std::mutex> lock(instance.m_mutex);
        const auto it = instance.m_eventFilters.find(EventType::Exception);
        shouldBroadcast = instance.m_eventsEnabled &&
                         it != instance.m_eventFilters.end() && it->second;
    }
    if (!shouldBroadcast) {
        return;
    }
    
    ExceptionEvent event;
    event.type = EventType::Exception;
    event.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    event.exceptionCode = code;
    event.exceptionAddress = address;
    event.firstChance = true;
    
    // 格式化异常名称
    std::stringstream ss;
    ss << "Exception 0x" << std::hex << std::setw(8) << std::setfill('0') << code;
    event.exceptionName = ss.str();
    
    instance.BroadcastEvent(event);
    Logger::Info("Exception 0x{:X} at 0x{:X}", code, address);
}

void EventCallbackHandler::OnModuleLoad(const char* name, uint64_t base, uint64_t size) {
    auto& instance = Instance();

    bool shouldBroadcast = false;
    {
        std::lock_guard<std::mutex> lock(instance.m_mutex);
        const auto it = instance.m_eventFilters.find(EventType::ModuleLoaded);
        shouldBroadcast = instance.m_eventsEnabled &&
                         it != instance.m_eventFilters.end() && it->second;
    }
    if (!shouldBroadcast) {
        return;
    }
    
    ModuleEvent event;
    event.type = EventType::ModuleLoaded;
    event.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    const std::string safeName = name ? name : "";
    event.moduleName = safeName;
    event.base = base;
    event.size = size;
    
    instance.BroadcastEvent(event);
    Logger::Info("Module loaded: {} at 0x{:X}", safeName, base);
}

void EventCallbackHandler::OnModuleUnload(const char* name) {
    auto& instance = Instance();

    bool shouldBroadcast = false;
    {
        std::lock_guard<std::mutex> lock(instance.m_mutex);
        const auto it = instance.m_eventFilters.find(EventType::ModuleUnloaded);
        shouldBroadcast = instance.m_eventsEnabled &&
                         it != instance.m_eventFilters.end() && it->second;
    }
    if (!shouldBroadcast) {
        return;
    }
    
    ModuleEvent event;
    event.type = EventType::ModuleUnloaded;
    event.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    const std::string safeName = name ? name : "";
    event.moduleName = safeName;
    event.base = 0;
    event.size = 0;
    
    instance.BroadcastEvent(event);
    Logger::Info("Module unloaded: {}", safeName);
}

void EventCallbackHandler::OnCreateProcess() {
    auto& instance = Instance();

    bool shouldBroadcast = false;
    {
        std::lock_guard<std::mutex> lock(instance.m_mutex);
        const auto it = instance.m_eventFilters.find(EventType::ProcessCreated);
        shouldBroadcast = instance.m_eventsEnabled &&
                         it != instance.m_eventFilters.end() && it->second;
    }
    if (!shouldBroadcast) {
        return;
    }
    
    // 创建基本事件
    struct ProcessEvent : public EventInfo {
        nlohmann::json ToJson() const override {
            nlohmann::json json;
            json["type"] = "process_created";
            json["timestamp"] = timestamp;
            return json;
        }
    };
    
    ProcessEvent event;
    event.type = EventType::ProcessCreated;
    event.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    
    instance.BroadcastEvent(event);
    Logger::Info("Process created");
}

void EventCallbackHandler::OnExitProcess() {
    auto& instance = Instance();

    bool shouldBroadcast = false;
    {
        std::lock_guard<std::mutex> lock(instance.m_mutex);
        const auto it = instance.m_eventFilters.find(EventType::ProcessExited);
        shouldBroadcast = instance.m_eventsEnabled &&
                         it != instance.m_eventFilters.end() && it->second;
    }
    if (!shouldBroadcast) {
        return;
    }
    
    struct ProcessEvent : public EventInfo {
        nlohmann::json ToJson() const override {
            nlohmann::json json;
            json["type"] = "process_exited";
            json["timestamp"] = timestamp;
            return json;
        }
    };
    
    ProcessEvent event;
    event.type = EventType::ProcessExited;
    event.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    
    instance.BroadcastEvent(event);
    Logger::Info("Process exited");
}

void EventCallbackHandler::BroadcastEvent(const EventInfo& event) {
    try {
        // 构建 JSON-RPC 通知
        nlohmann::json notification;
        notification["jsonrpc"] = "2.0";
        notification["method"] = "debug.event";
        notification["params"] = event.ToJson();
        
        // TODO: 事件通知需要通过 MCPHttpServer 的 SSE 机制发送
        // 暂时禁用，等待重新实现
        // ServerManager::Instance().SendNotification("debug.event", event.ToJson());
        
        Logger::Trace("Event recorded: {}", EventTypeToString(event.type));
    } catch (const std::exception& e) {
        Logger::Error("Failed to record event: {}", e.what());
    }
}

std::string EventCallbackHandler::EventTypeToString(EventType type) {
    switch (type) {
        case EventType::Breakpoint: return "Breakpoint";
        case EventType::Exception: return "Exception";
        case EventType::ProcessCreated: return "ProcessCreated";
        case EventType::ProcessExited: return "ProcessExited";
        case EventType::ThreadCreated: return "ThreadCreated";
        case EventType::ThreadExited: return "ThreadExited";
        case EventType::ModuleLoaded: return "ModuleLoaded";
        case EventType::ModuleUnloaded: return "ModuleUnloaded";
        case EventType::DebugEvent: return "DebugEvent";
        default: return "Unknown";
    }
}

// ==================== Event ToJson 实现 ====================

nlohmann::json BreakpointEvent::ToJson() const {
    nlohmann::json json;
    json["type"] = "breakpoint";
    json["timestamp"] = timestamp;
    json["address"] = StringUtils::FormatAddress(address);
    
    if (!name.empty()) {
        json["name"] = name;
    }
    
    json["hit_count"] = hitCount;
    
    return json;
}

nlohmann::json ExceptionEvent::ToJson() const {
    nlohmann::json json;
    json["type"] = "exception";
    json["timestamp"] = timestamp;
    
    // 格式化异常代码
    std::stringstream ss1;
    ss1 << "0x" << std::hex << std::setw(8) << std::setfill('0') << exceptionCode;
    json["code"] = ss1.str();
    
    json["address"] = StringUtils::FormatAddress(exceptionAddress);
    json["name"] = exceptionName;
    json["first_chance"] = firstChance;
    
    return json;
}

nlohmann::json ModuleEvent::ToJson() const {
    nlohmann::json json;
    
    if (type == EventType::ModuleLoaded) {
        json["type"] = "module_loaded";
    } else {
        json["type"] = "module_unloaded";
    }
    
    json["timestamp"] = timestamp;
    json["name"] = moduleName;
    
    if (!modulePath.empty()) {
        json["path"] = modulePath;
    }
    
    if (base != 0) {
        json["base"] = StringUtils::FormatAddress(base);
        json["size"] = size;
    }
    
    return json;
}

} // namespace MCP
