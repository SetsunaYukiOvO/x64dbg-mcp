#include "RegisterHandler.h"
#include "../business/RegisterManager.h"
#include "../core/MethodDispatcher.h"
#include "../core/RequestValidator.h"
#include "../core/PermissionChecker.h"
#include "../core/Exceptions.h"
#include "../core/Logger.h"
#include "../utils/StringUtils.h"

namespace MCP {

void RegisterHandler::RegisterMethods() {
    auto& dispatcher = MethodDispatcher::Instance();
    
    dispatcher.RegisterMethod("register.get", Get);
    dispatcher.RegisterMethod("register.set", Set);
    dispatcher.RegisterMethod("register.list", List);
    dispatcher.RegisterMethod("register.get_batch", GetBatch);
    
    Logger::Info("Registered register.* methods");
}

json RegisterHandler::Get(const json& params) {
    RequestValidator::RequireString(params, "name");
    
    std::string name = params["name"].get<std::string>();
    
    auto& manager = RegisterManager::Instance();
    auto info = manager.GetRegisterInfo(name);
    
    return {
        {"name", info.name},
        {"value", StringUtils::FormatAddress(info.value)},
        {"value_decimal", info.value},
        {"size", info.size}
    };
}

json RegisterHandler::Set(const json& params) {
    // 检查权限
    if (!PermissionChecker::Instance().IsRegisterWriteAllowed()) {
        throw PermissionDeniedException("Register write not allowed");
    }
    
    RequestValidator::RequireString(params, "name");
    RequestValidator::RequireField(params, "value");
    
    std::string name = params["name"].get<std::string>();
    
    uint64_t value;
    if (params["value"].is_string()) {
        try {
            value = StringUtils::ParseAddress(params["value"].get<std::string>());
        } catch (const std::exception&) {
            throw InvalidParamsException("Invalid register value");
        }
    } else if (params["value"].is_number_unsigned()) {
        value = params["value"].get<uint64_t>();
    } else if (params["value"].is_number_integer()) {
        const int64_t signedValue = params["value"].get<int64_t>();
        if (signedValue < 0) {
            throw InvalidParamsException("Register value cannot be negative");
        }
        value = static_cast<uint64_t>(signedValue);
    } else {
        throw InvalidParamsException("Value must be a string or integer");
    }
    
    auto& manager = RegisterManager::Instance();
    bool success = manager.SetRegister(name, value);
    
    return {
        {"success", success},
        {"name", name},
        {"value", StringUtils::FormatAddress(value)}
    };
}

json RegisterHandler::List(const json& params) {
    auto& manager = RegisterManager::Instance();
    
    // 检查是否只列出通用寄存器
    bool generalOnly = RequestValidator::GetBoolean(params, "general_only", false);
    
    std::vector<RegisterInfo> registers;
    if (generalOnly) {
        registers = manager.GetGeneralRegisters();
    } else {
        registers = manager.ListAllRegisters();
    }
    
    json registerArray = json::array();
    for (const auto& reg : registers) {
        registerArray.push_back({
            {"name", reg.name},
            {"value", StringUtils::FormatAddress(reg.value)},
            {"value_decimal", reg.value},
            {"size", reg.size}
        });
    }
    
    return {
        {"registers", registerArray},
        {"count", registers.size()}
    };
}

json RegisterHandler::GetBatch(const json& params) {
    RequestValidator::RequireArray(params, "names");
    
    auto& manager = RegisterManager::Instance();
    json results = json::array();
    
    for (const auto& nameJson : params["names"]) {
        if (!nameJson.is_string()) {
            continue;
        }
        
        std::string name = nameJson.get<std::string>();
        
        try {
            auto info = manager.GetRegisterInfo(name);
            results.push_back({
                {"name", info.name},
                {"value", StringUtils::FormatAddress(info.value)},
                {"value_decimal", info.value},
                {"size", info.size},
                {"success", true}
            });
        } catch (const std::exception& ex) {
            results.push_back({
                {"name", name},
                {"success", false},
                {"error", ex.what()}
            });
        }
    }
    
    return {
        {"registers", results},
        {"count", results.size()}
    };
}

} // namespace MCP
