#include "EventCallbackHandler.h"
#include "../core/Logger.h"
#include "../core/ConfigManager.h"
#include "../communication/MCPHttpServer.h"
#include "../utils/StringUtils.h"
#include <chrono>
#include <sstream>
#include <iomanip>

namespace MCP {

namespace {

uint64_t CurrentTimestampMs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
}

} // namespace

EventCallbackHandler& EventCallbackHandler::Instance() {
    static EventCallbackHandler instance;
    return instance;
}

void EventCallbackHandler::Initialize() {
    Logger::Info("Initializing event callback handler");
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 默认启用所有事�?
    m_eventsEnabled = ConfigManager::Instance().Get<bool>("features.enable_notifications", true);
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
    event.timestamp = CurrentTimestampMs();
    event.address = address;
    event.hitCount = 0;  // 从断点管理器获取
    
    instance.BroadcastEvent(event);
    Logger::Debug("Breakpoint hit at 0x{:X}", address);
}

void EventCallbackHandler::OnException(uint32_t code, uint64_t address, bool firstChance) {
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
    event.timestamp = CurrentTimestampMs();
    event.exceptionCode = code;
    event.exceptionAddress = address;
    event.firstChance = firstChance;
    
    // 格式化异常名�?
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
    event.timestamp = CurrentTimestampMs();
    const std::string safeName = name ? name : "";
    event.moduleName = safeName;
    event.base = base;
    event.size = size;
    
    instance.BroadcastEvent(event);
    Logger::Info("Module loaded: {} at 0x{:X}", safeName, base);
}

void EventCallbackHandler::OnModuleUnload(const char* name, uint64_t base) {
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
    event.timestamp = CurrentTimestampMs();
    const std::string safeName = name ? name : "";
    event.moduleName = safeName;
    event.base = base;
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
    event.timestamp = CurrentTimestampMs();
    
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
    event.timestamp = CurrentTimestampMs();
    
    instance.BroadcastEvent(event);
    Logger::Info("Process exited");
}

void EventCallbackHandler::BroadcastEvent(const EventInfo& event) {
    try {
        const nlohmann::json payload = event.ToJson();
        const bool delivered = MCPHttpServer::BroadcastNotification("debug.event", payload);
        if (delivered) {
            Logger::Trace("Event delivered: {}", EventTypeToString(event.type));
        } else {
            Logger::Trace("Event queued without active SSE clients: {}", EventTypeToString(event.type));
        }
    } catch (const std::exception& e) {
        Logger::Error("Failed to broadcast event: {}", e.what());
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
    
    // 格式化异常代�?
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

