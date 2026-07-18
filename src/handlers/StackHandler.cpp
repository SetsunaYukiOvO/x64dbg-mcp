#include "StackHandler.h"
#include "../business/StackManager.h"
#include "../core/ArchitectureRegisterNames.h"
#include "../core/Exceptions.h"
#include "../core/Logger.h"
#include "../core/MethodDispatcher.h"
#include "../core/RequestValidator.h"
#include "../utils/StringUtils.h"
#include <iomanip>
#include <sstream>

namespace MCP {

void StackHandler::RegisterMethods() {
    LOG_INFO("Registering stack methods...");
    auto& dispatcher = MethodDispatcher::Instance();
    
    dispatcher.RegisterMethod("stack.get_trace", GetStackTrace);
    dispatcher.RegisterMethod("stack.read_frame", ReadStackFrame);
    dispatcher.RegisterMethod("stack.get_pointers", GetStackPointers);
    dispatcher.RegisterMethod("stack.is_on_stack", IsOnStack);
    
    LOG_INFO("Stack methods registered");
}

nlohmann::json StackHandler::GetStackTrace(const nlohmann::json& params) {
    LOG_DEBUG("StackHandler::GetStackTrace called");
    
    // 解析参数
    size_t maxDepth = 0;  // 0 = 无限制
    if (params.contains("max_depth") && params["max_depth"].is_number()) {
        maxDepth = params["max_depth"].get<size_t>();
    }
    
    auto& stackMgr = StackManager::Instance();
    auto frames = stackMgr.GetStackTrace(maxDepth);
    
    // 构建响应
    nlohmann::json result;
    result["frames"] = nlohmann::json::array();
    result["count"] = frames.size();
    
    for (const auto& frame : frames) {
        result["frames"].push_back(FormatStackFrame(frame));
    }
    
    LOG_DEBUG("Returned {} stack frames", frames.size());
    return result;
}

nlohmann::json StackHandler::ReadStackFrame(const nlohmann::json& params) {
    LOG_DEBUG("StackHandler::ReadStackFrame called");
    
    // 验证参数
    if (!params.contains("address")) {
        throw InvalidParamsException("Missing required parameter: address");
    }
    
    if (!params.contains("size")) {
        throw InvalidParamsException("Missing required parameter: size");
    }
    
    // 解析地址
    uint64_t address = RequestValidator::GetAddress(params, "address");
    
    size_t size = params["size"].get<size_t>();
    
    // 限制读取大小（防止过大请求）
    if (size > 64 * 1024) {  // 64KB 最大
        throw InvalidParamsException("Size too large (max 64KB)");
    }
    
    auto& stackMgr = StackManager::Instance();
    auto data = stackMgr.ReadStackFrame(address, size);
    
    // 构建响应
    nlohmann::json result;
    result["address"] = StringUtils::FormatAddress(address);
    result["size"] = data.size();
    result["data"] = StringUtils::ToBase64(data);
    result["encoding"] = "base64";
    
    return result;
}

nlohmann::json StackHandler::GetStackPointers(const nlohmann::json& params) {
    LOG_DEBUG("StackHandler::GetStackPointers called");
    
    auto& stackMgr = StackManager::Instance();
    
    uint64_t rsp = stackMgr.GetStackPointer();
    uint64_t rbp = stackMgr.GetBasePointer();
    
    nlohmann::json result;
    result[ArchitectureRegisterNames::StackPointer] = StringUtils::FormatAddress(rsp);
    result[ArchitectureRegisterNames::BasePointer] = StringUtils::FormatAddress(rbp);
    result["on_stack"] = true;  // 当前总是在栈上
    
    return result;
}

nlohmann::json StackHandler::IsOnStack(const nlohmann::json& params) {
    LOG_DEBUG("StackHandler::IsOnStack called");
    
    // 验证参数
    if (!params.contains("address")) {
        throw InvalidParamsException("Missing required parameter: address");
    }
    
    uint64_t address = RequestValidator::GetAddress(params, "address");
    
    auto& stackMgr = StackManager::Instance();
    bool onStack = stackMgr.IsAddressOnStack(address);
    
    nlohmann::json result;
    result["address"] = StringUtils::FormatAddress(address);
    result["on_stack"] = onStack;
    
    return result;
}

nlohmann::json StackHandler::FormatStackFrame(const StackFrame& frame) {
    nlohmann::json j;
    
    j["address"] = StringUtils::FormatAddress(frame.address);
    j["from"] = StringUtils::FormatAddress(frame.from);
    j["to"] = StringUtils::FormatAddress(frame.to);
    j["comment"] = frame.comment;
    j[ArchitectureRegisterNames::StackPointer] = StringUtils::FormatAddress(frame.rsp);
    j[ArchitectureRegisterNames::BasePointer] = StringUtils::FormatAddress(frame.rbp);
    j["is_user"] = frame.isUser;
    j["party"] = frame.party;
    
    return j;
}

} // namespace MCP
