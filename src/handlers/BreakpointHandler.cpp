#include "BreakpointHandler.h"
#include "../business/BreakpointManager.h"
#include "../core/MethodDispatcher.h"
#include "../core/RequestValidator.h"
#include "../core/PermissionChecker.h"
#include "../core/Exceptions.h"
#include "../utils/StringUtils.h"
#include <limits>

namespace MCP {

void BreakpointHandler::RegisterMethods() {
    auto& dispatcher = MethodDispatcher::Instance();
    
    dispatcher.RegisterMethod("breakpoint.set", Set);
    dispatcher.RegisterMethod("breakpoint.delete", Delete);
    dispatcher.RegisterMethod("breakpoint.enable", Enable);
    dispatcher.RegisterMethod("breakpoint.disable", Disable);
    dispatcher.RegisterMethod("breakpoint.toggle", Toggle);
    dispatcher.RegisterMethod("breakpoint.list", List);
    dispatcher.RegisterMethod("breakpoint.get", Get);
    dispatcher.RegisterMethod("breakpoint.delete_all", DeleteAll);
    dispatcher.RegisterMethod("breakpoint.set_condition", SetCondition);
    dispatcher.RegisterMethod("breakpoint.set_log", SetLog);
    dispatcher.RegisterMethod("breakpoint.reset_hitcount", ResetHitCount);
}

nlohmann::json BreakpointHandler::Set(const nlohmann::json& params) {
    // 妫€鏌ュ啓鏉冮檺
    if (!PermissionChecker::Instance().IsBreakpointModificationAllowed()) {
        throw PermissionDeniedException("Setting breakpoint requires write permission");
    }
    
    // 楠岃瘉鍙傛暟
    if (!params.contains("address")) {
        throw InvalidParamsException(
            "Missing required parameter: address. "
            "Example: {\"address\":\"0x401000\",\"type\":\"software\"}"
        );
    }
    
    const uint64_t address = RequestValidator::GetAddress(params, "address");
    
    std::string typeStr = params.value("type", "software");
    std::string name = params.value("name", "");

    if (typeStr != "hardware" &&
        (params.contains("hw_condition") || params.contains("hw_size"))) {
        throw InvalidParamsException("Hardware breakpoint parameters require type=hardware");
    }
    if (typeStr != "memory" && params.contains("mem_size")) {
        throw InvalidParamsException("Memory breakpoint size requires type=memory");
    }

    auto& manager = BreakpointManager::Instance();
    bool success = false;
    
    if (typeStr == "software") {
        success = manager.SetSoftwareBreakpoint(address, name);
    }
    else if (typeStr == "hardware") {
        if (params.contains("hw_condition") && !params["hw_condition"].is_string()) {
            throw InvalidParamsException("Hardware breakpoint condition must be a string");
        }
        int hwSize = 1;
        if (params.contains("hw_size")) {
            if (!params["hw_size"].is_number_integer()) {
                throw InvalidParamsException("Hardware breakpoint size must be an integer");
            }
            if (params["hw_size"] == 1) {
                hwSize = 1;
            } else if (params["hw_size"] == 2) {
                hwSize = 2;
            } else if (params["hw_size"] == 4) {
                hwSize = 4;
            } else if (params["hw_size"] == 8) {
                hwSize = 8;
            } else {
                throw InvalidParamsException("Hardware breakpoint size must be 1, 2, 4, or 8 bytes");
            }
        }
        std::string hwCondStr = params.value("hw_condition", "execute");

        HardwareBreakpointCondition hwCond;
        if (hwCondStr == "execute") {
            hwCond = HardwareBreakpointCondition::Execute;
        } else if (hwCondStr == "write") {
            hwCond = HardwareBreakpointCondition::Write;
        } else if (hwCondStr == "readwrite" || hwCondStr == "access") {
            hwCond = HardwareBreakpointCondition::ReadWrite;
        } else {
            throw InvalidParamsException("Invalid hardware breakpoint condition: " + hwCondStr);
        }

        HardwareBreakpointSize hwSz;
        switch (hwSize) {
            case 1: hwSz = HardwareBreakpointSize::Byte1; break;
            case 2: hwSz = HardwareBreakpointSize::Byte2; break;
            case 4: hwSz = HardwareBreakpointSize::Byte4; break;
            case 8:
#ifdef XDBG_ARCH_X64
                hwSz = HardwareBreakpointSize::Byte8;
                break;
#else
                throw InvalidParamsException("8-byte hardware breakpoints are only supported on x64");
#endif
            default:
                throw InvalidParamsException("Hardware breakpoint size must be 1, 2, 4, or 8 bytes");
        }
        if (hwCond == HardwareBreakpointCondition::Execute && hwSize != 1) {
            throw InvalidParamsException("Execute hardware breakpoints must use a size of 1 byte");
        }
        if (address % static_cast<uint64_t>(hwSize) != 0) {
            throw InvalidParamsException("Hardware breakpoint address must be aligned to its size");
        }

        success = manager.SetHardwareBreakpoint(address, hwCond, hwSz, name);
    }
    else if (typeStr == "memory") {
        if (params.contains("mem_size") &&
            (!params["mem_size"].is_number_integer() || params["mem_size"] < 1)) {
            throw InvalidParamsException("Memory breakpoint size must be a positive integer");
        }
        const uint64_t memSizeValue = params.value("mem_size", UINT64_C(1));
        if (memSizeValue == 0 || memSizeValue > std::numeric_limits<size_t>::max()) {
            throw InvalidParamsException("Memory breakpoint size must fit the host size type and be greater than zero");
        }
        success = manager.SetMemoryBreakpoint(address, static_cast<size_t>(memSizeValue), name);
    }
    else {
        throw InvalidParamsException("Invalid breakpoint type: " + typeStr);
    }
    
    if (!success) {
        throw MCPException("Failed to set breakpoint at: " + StringUtils::FormatAddress(address));
    }
    
    // 濡傛灉鏈夋潯浠?璁剧疆鏉′欢
    if (params.contains("condition")) {
        std::string condition = params["condition"].get<std::string>();
        manager.SetCondition(address, condition);
    }
    
    // 鏋勫缓鍝嶅簲
    nlohmann::json result;
    result["address"] = StringUtils::FormatAddress(address);
    result["type"] = typeStr;
    result["enabled"] = true;
    
    if (!name.empty()) {
        result["name"] = name;
    }
    
    return result;
}

nlohmann::json BreakpointHandler::Delete(const nlohmann::json& params) {
    // 妫€鏌ュ啓鏉冮檺
    if (!PermissionChecker::Instance().IsBreakpointModificationAllowed()) {
        throw PermissionDeniedException("Deleting breakpoint requires write permission");
    }
    
    // 楠岃瘉鍙傛暟
    if (!params.contains("address")) {
        throw InvalidParamsException("Missing required parameter: address");
    }
    
    const uint64_t address = RequestValidator::GetAddress(params, "address");
    
    std::optional<BreakpointType> typeFilter;
    if (params.contains("type")) {
        std::string typeStr = params["type"].get<std::string>();
        if (typeStr == "software") {
            typeFilter = BreakpointType::Software;
        } else if (typeStr == "hardware") {
            typeFilter = BreakpointType::Hardware;
        } else if (typeStr == "memory") {
            typeFilter = BreakpointType::Memory;
        } else {
            throw InvalidParamsException("Invalid breakpoint type: " + typeStr);
        }
    }

    auto& manager = BreakpointManager::Instance();
    bool success = manager.DeleteBreakpoint(address, typeFilter);
    
    if (!success) {
        throw MCPException("Failed to delete breakpoint at: " + StringUtils::FormatAddress(address));
    }
    
    nlohmann::json result;
    result["success"] = true;
    result["address"] = StringUtils::FormatAddress(address);
    
    return result;
}

nlohmann::json BreakpointHandler::Enable(const nlohmann::json& params) {
    // 妫€鏌ュ啓鏉冮檺
    if (!PermissionChecker::Instance().IsBreakpointModificationAllowed()) {
        throw PermissionDeniedException("Enabling breakpoint requires write permission");
    }
    
    if (!params.contains("address")) {
        throw InvalidParamsException("Missing required parameter: address");
    }
    
    const uint64_t address = RequestValidator::GetAddress(params, "address");
    
    auto& manager = BreakpointManager::Instance();
    bool success = manager.EnableBreakpoint(address);
    
    if (!success) {
        throw MCPException("Failed to enable breakpoint at: " + StringUtils::FormatAddress(address));
    }
    
    nlohmann::json result;
    result["success"] = true;
    result["address"] = StringUtils::FormatAddress(address);
    
    return result;
}

nlohmann::json BreakpointHandler::Disable(const nlohmann::json& params) {
    // 妫€鏌ュ啓鏉冮檺
    if (!PermissionChecker::Instance().IsBreakpointModificationAllowed()) {
        throw PermissionDeniedException("Disabling breakpoint requires write permission");
    }
    
    if (!params.contains("address")) {
        throw InvalidParamsException("Missing required parameter: address");
    }
    
    const uint64_t address = RequestValidator::GetAddress(params, "address");
    
    auto& manager = BreakpointManager::Instance();
    bool success = manager.DisableBreakpoint(address);
    
    if (!success) {
        throw MCPException("Failed to disable breakpoint at: " + StringUtils::FormatAddress(address));
    }
    
    nlohmann::json result;
    result["success"] = true;
    result["address"] = StringUtils::FormatAddress(address);
    
    return result;
}

nlohmann::json BreakpointHandler::Toggle(const nlohmann::json& params) {
    // 妫€鏌ュ啓鏉冮檺
    if (!PermissionChecker::Instance().IsBreakpointModificationAllowed()) {
        throw PermissionDeniedException("Toggling breakpoint requires write permission");
    }
    
    if (!params.contains("address")) {
        throw InvalidParamsException("Missing required parameter: address");
    }
    
    const uint64_t address = RequestValidator::GetAddress(params, "address");
    
    auto& manager = BreakpointManager::Instance();
    bool success = manager.ToggleBreakpoint(address);
    
    if (!success) {
        throw MCPException("Failed to toggle breakpoint at: " + StringUtils::FormatAddress(address));
    }
    
    // 鑾峰彇褰撳墠鐘舵€?
    auto bp = manager.GetBreakpoint(address);
    
    nlohmann::json result;
    result["success"] = true;
    result["address"] = StringUtils::FormatAddress(address);
    result["enabled"] = bp.has_value() ? bp->enabled : false;
    
    return result;
}

nlohmann::json BreakpointHandler::List(const nlohmann::json& params) {
    std::optional<BreakpointType> typeFilter;
    
    if (params.contains("type")) {
        std::string typeStr = params["type"].get<std::string>();
        if (typeStr == "software") {
            typeFilter = BreakpointType::Software;
        } else if (typeStr == "hardware") {
            typeFilter = BreakpointType::Hardware;
        } else if (typeStr == "memory") {
            typeFilter = BreakpointType::Memory;
        } else {
            throw InvalidParamsException("Invalid breakpoint type: " + typeStr);
        }
    }

    auto& manager = BreakpointManager::Instance();
    auto breakpoints = manager.ListBreakpoints(typeFilter);
    
    nlohmann::json bpArray = nlohmann::json::array();
    for (const auto& bp : breakpoints) {
        bpArray.push_back(BreakpointInfoToJson(bp));
    }
    
    nlohmann::json result;
    result["count"] = breakpoints.size();
    result["breakpoints"] = bpArray;
    
    return result;
}

nlohmann::json BreakpointHandler::Get(const nlohmann::json& params) {
    if (!params.contains("address")) {
        throw InvalidParamsException("Missing required parameter: address");
    }
    
    const uint64_t address = RequestValidator::GetAddress(params, "address");
    
    auto& manager = BreakpointManager::Instance();
    auto bp = manager.GetBreakpoint(address);
    
    if (!bp.has_value()) {
        throw InvalidAddressException("No breakpoint at address: " + StringUtils::FormatAddress(address));
    }
    
    return BreakpointInfoToJson(bp.value());
}

nlohmann::json BreakpointHandler::DeleteAll(const nlohmann::json& params) {
    // 妫€鏌ュ啓鏉冮檺
    if (!PermissionChecker::Instance().IsBreakpointModificationAllowed()) {
        throw PermissionDeniedException("Deleting breakpoints requires write permission");
    }
    
    std::optional<BreakpointType> typeFilter;
    
    if (params.contains("type")) {
        std::string typeStr = params["type"].get<std::string>();
        if (typeStr == "software") {
            typeFilter = BreakpointType::Software;
        } else if (typeStr == "hardware") {
            typeFilter = BreakpointType::Hardware;
        } else if (typeStr == "memory") {
            typeFilter = BreakpointType::Memory;
        } else {
            throw InvalidParamsException("Invalid breakpoint type: " + typeStr);
        }
    }

    auto& manager = BreakpointManager::Instance();
    size_t deletedCount = manager.DeleteAllBreakpoints(typeFilter);
    
    nlohmann::json result;
    result["deleted_count"] = deletedCount;
    
    return result;
}

nlohmann::json BreakpointHandler::SetCondition(const nlohmann::json& params) {
    // 妫€鏌ュ啓鏉冮檺
    if (!PermissionChecker::Instance().IsBreakpointModificationAllowed()) {
        throw PermissionDeniedException("Setting breakpoint condition requires write permission");
    }
    // Breakpoint conditions are evaluated as x64dbg expressions each time
    // the breakpoint is hit, so also require script execution permission.
    if (!PermissionChecker::Instance().IsScriptExecutionAllowed()) {
        throw PermissionDeniedException("Setting breakpoint condition requires script execution permission");
    }

    if (!params.contains("address")) {
        throw InvalidParamsException("Missing required parameter: address");
    }
    if (!params.contains("condition")) {
        throw InvalidParamsException("Missing required parameter: condition");
    }

    const uint64_t address = RequestValidator::GetAddress(params, "address");
    std::string condition = params["condition"].get<std::string>();

    auto& manager = BreakpointManager::Instance();
    bool success = manager.SetCondition(address, condition);
    
    if (!success) {
        throw MCPException("Failed to set condition for breakpoint at: " + StringUtils::FormatAddress(address));
    }
    
    nlohmann::json result;
    result["success"] = true;
    result["address"] = StringUtils::FormatAddress(address);
    result["condition"] = condition;
    
    return result;
}

nlohmann::json BreakpointHandler::SetLog(const nlohmann::json& params) {
    // 妫€鏌ュ啓鏉冮檺
    if (!PermissionChecker::Instance().IsBreakpointModificationAllowed()) {
        throw PermissionDeniedException("Setting log breakpoint requires write permission");
    }
    // Log text is evaluated by x64dbg and may contain embedded command syntax,
    // so also require script execution permission.
    if (!PermissionChecker::Instance().IsScriptExecutionAllowed()) {
        throw PermissionDeniedException("Setting breakpoint log text requires script execution permission");
    }

    if (!params.contains("address")) {
        throw InvalidParamsException("Missing required parameter: address");
    }
    if (!params.contains("log_text")) {
        throw InvalidParamsException("Missing required parameter: log_text");
    }

    const uint64_t address = RequestValidator::GetAddress(params, "address");
    std::string message = params["log_text"].get<std::string>();

    auto& manager = BreakpointManager::Instance();
    bool success = manager.SetLogBreakpoint(address, message);
    
    if (!success) {
        throw MCPException("Failed to set log breakpoint at: " + StringUtils::FormatAddress(address));
    }
    
    nlohmann::json result;
    result["success"] = true;
    result["address"] = StringUtils::FormatAddress(address);
    result["message"] = message;
    
    return result;
}

nlohmann::json BreakpointHandler::ResetHitCount(const nlohmann::json& params) {
    // 妫€鏌ュ啓鏉冮檺
    if (!PermissionChecker::Instance().IsBreakpointModificationAllowed()) {
        throw PermissionDeniedException("Resetting breakpoint hit count requires write permission");
    }
    
    if (!params.contains("address")) {
        throw InvalidParamsException("Missing required parameter: address");
    }
    
    const uint64_t address = RequestValidator::GetAddress(params, "address");
    
    auto& manager = BreakpointManager::Instance();
    bool success = manager.ResetHitCount(address);
    
    if (!success) {
        throw MCPException("Failed to reset hit count for breakpoint at: " + StringUtils::FormatAddress(address));
    }
    
    nlohmann::json result;
    result["success"] = true;
    result["address"] = StringUtils::FormatAddress(address);
    result["hit_count"] = 0;
    
    return result;
}

nlohmann::json BreakpointHandler::BreakpointInfoToJson(const BreakpointInfo& bp) {
    nlohmann::json json;
    
    json["address"] = StringUtils::FormatAddress(bp.address);
    
    // 绫诲瀷
    switch (bp.type) {
        case BreakpointType::Software:
            json["type"] = "software";
            break;
        case BreakpointType::Hardware:
            json["type"] = "hardware";
            
            // 纭欢鏂偣鐗规湁灞炴€?
            switch (bp.condition) {
                case HardwareBreakpointCondition::Execute:
                    json["hw_condition"] = "execute";
                    break;
                case HardwareBreakpointCondition::Write:
                    json["hw_condition"] = "write";
                    break;
                case HardwareBreakpointCondition::ReadWrite:
                    json["hw_condition"] = "readwrite";
                    break;
            }
            json["hw_size"] = static_cast<int>(bp.size);
            break;
        case BreakpointType::Memory:
            json["type"] = "memory";
            json["mem_size"] = bp.memorySize;
            break;
    }
    
    json["enabled"] = bp.enabled;
    json["hit_count"] = bp.hitCount;
    
    if (!bp.name.empty()) {
        json["name"] = bp.name;
    }
    
    if (!bp.module.empty()) {
        json["module"] = bp.module;
    }
    
    if (!bp.condition_expr.empty()) {
        json["condition"] = bp.condition_expr;
    }
    
    if (bp.isLogBreakpoint) {
        json["is_log_breakpoint"] = true;
        json["log_message"] = bp.logMessage;
    }
    
    return json;
}

} // namespace MCP

