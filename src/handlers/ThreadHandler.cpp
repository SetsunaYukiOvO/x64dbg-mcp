#include "ThreadHandler.h"
#include "../business/ThreadManager.h"
#include "../core/ArchitectureRegisterNames.h"
#include "../core/Exceptions.h"
#include "../core/Logger.h"
#include "../core/MethodDispatcher.h"
#include "../utils/StringUtils.h"

namespace MCP {

void ThreadHandler::RegisterMethods() {
    LOG_INFO("Registering thread methods...");
    auto& dispatcher = MethodDispatcher::Instance();
    
    dispatcher.RegisterMethod("thread.list", ListThreads);
    dispatcher.RegisterMethod("thread.get_current", GetCurrentThread);
    dispatcher.RegisterMethod("thread.get", GetThread);
    dispatcher.RegisterMethod("thread.switch", SwitchThread);
    dispatcher.RegisterMethod("thread.suspend", SuspendThread);
    dispatcher.RegisterMethod("thread.resume", ResumeThread);
    dispatcher.RegisterMethod("thread.get_count", GetThreadCount);
    
    LOG_INFO("Thread methods registered");
}

nlohmann::json ThreadHandler::ListThreads(const nlohmann::json& params) {
    LOG_DEBUG("ThreadHandler::ListThreads called");
    
    auto& threadMgr = ThreadManager::Instance();
    auto threads = threadMgr.GetThreadList();
    
    // Build response.
    nlohmann::json result;
    result["threads"] = nlohmann::json::array();
    result["count"] = threads.size();
    
    for (const auto& thread : threads) {
        result["threads"].push_back(FormatThreadInfo(thread));
    }
    
    LOG_DEBUG("Returned {} threads", threads.size());
    return result;
}

nlohmann::json ThreadHandler::GetCurrentThread(const nlohmann::json& params) {
    LOG_DEBUG("ThreadHandler::GetCurrentThread called");
    
    auto& threadMgr = ThreadManager::Instance();
    auto thread = threadMgr.GetCurrentThread();
    
    return FormatThreadInfo(thread);
}

nlohmann::json ThreadHandler::GetThread(const nlohmann::json& params) {
    LOG_DEBUG("ThreadHandler::GetThread called");
    
    // Validate parameters.
    if (!params.contains("thread_id")) {
        throw InvalidParamsException("Missing required parameter: thread_id");
    }
    
    uint32_t threadId = params["thread_id"].get<uint32_t>();
    
    auto& threadMgr = ThreadManager::Instance();
    auto thread = threadMgr.GetThread(threadId);
    
    return FormatThreadInfo(thread);
}

nlohmann::json ThreadHandler::SwitchThread(const nlohmann::json& params) {
    LOG_DEBUG("ThreadHandler::SwitchThread called");
    
    // Validate parameters.
    if (!params.contains("thread_id")) {
        throw InvalidParamsException("Missing required parameter: thread_id");
    }
    
    uint32_t threadId = params["thread_id"].get<uint32_t>();
    
    auto& threadMgr = ThreadManager::Instance();
    
    // Record previous thread ID.
    uint32_t previousId = threadMgr.GetCurrentThreadId();
    
    // Execute switch.
    bool success = threadMgr.SwitchThread(threadId);
    uint32_t currentId = previousId;
    try {
        currentId = threadMgr.GetCurrentThreadId();
    } catch (const std::exception& e) {
        LOG_WARNING("Failed to query current thread after switch: {}", e.what());
    }

    // Build response.
    nlohmann::json result;
    result["success"] = success;
    result["previous_id"] = previousId;
    result["requested_id"] = threadId;
    result["current_id"] = currentId;

    if (success) {
        LOG_INFO("Switched from thread {} to thread {} (requested={})", previousId, currentId, threadId);
    }
    
    return result;
}

nlohmann::json ThreadHandler::SuspendThread(const nlohmann::json& params) {
    LOG_DEBUG("ThreadHandler::SuspendThread called");
    
    // Validate parameters.
    if (!params.contains("thread_id")) {
        throw InvalidParamsException("Missing required parameter: thread_id");
    }
    
    uint32_t threadId = params["thread_id"].get<uint32_t>();
    
    auto& threadMgr = ThreadManager::Instance();
    bool success = threadMgr.SuspendThread(threadId);
    
    nlohmann::json result;
    result["success"] = success;
    result["thread_id"] = threadId;
    
    if (success) {
        LOG_INFO("Suspended thread {}", threadId);
    }
    
    return result;
}

nlohmann::json ThreadHandler::ResumeThread(const nlohmann::json& params) {
    LOG_DEBUG("ThreadHandler::ResumeThread called");
    
    // Validate parameters.
    if (!params.contains("thread_id")) {
        throw InvalidParamsException("Missing required parameter: thread_id");
    }
    
    uint32_t threadId = params["thread_id"].get<uint32_t>();
    
    auto& threadMgr = ThreadManager::Instance();
    bool success = threadMgr.ResumeThread(threadId);
    
    nlohmann::json result;
    result["success"] = success;
    result["thread_id"] = threadId;
    
    if (success) {
        LOG_INFO("Resumed thread {}", threadId);
    }
    
    return result;
}

nlohmann::json ThreadHandler::GetThreadCount(const nlohmann::json& params) {
    LOG_DEBUG("ThreadHandler::GetThreadCount called");
    
    auto& threadMgr = ThreadManager::Instance();
    size_t count = threadMgr.GetThreadCount();
    
    nlohmann::json result;
    result["count"] = count;
    
    return result;
}

nlohmann::json ThreadHandler::FormatThreadInfo(const ThreadInfo& info) {
    nlohmann::json j;
    
    j["id"] = info.id;
    j["handle"] = info.handle;
    j["entry"] = StringUtils::FormatAddress(info.entry);
    j["teb"] = StringUtils::FormatAddress(info.teb);
    j["name"] = info.name;
    j["is_current"] = info.isCurrent;
    j["is_suspended"] = info.isSuspended;
    j["priority"] = info.priority;
    
    // Include register context when available.
    j[ArchitectureRegisterNames::InstructionPointer] = StringUtils::FormatAddress(info.rip);
    j[ArchitectureRegisterNames::StackPointer] = StringUtils::FormatAddress(info.rsp);
    j[ArchitectureRegisterNames::BasePointer] = StringUtils::FormatAddress(info.rbp);
    
    return j;
}

} // namespace MCP
