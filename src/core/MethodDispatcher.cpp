#include "MethodDispatcher.h"
#include "PermissionChecker.h"
#include "ResponseBuilder.h"
#include "Exceptions.h"
#include "Logger.h"

namespace MCP {

MethodDispatcher& MethodDispatcher::Instance() {
    static MethodDispatcher instance;
    return instance;
}

void MethodDispatcher::RegisterMethod(const std::string& method, MethodHandler handler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_handlers[method] = handler;
    Logger::Debug("Registered method: {}", method);
}

void MethodDispatcher::UnregisterMethod(const std::string& method) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_handlers.erase(method);
    Logger::Debug("Unregistered method: {}", method);
}

bool MethodDispatcher::IsMethodRegistered(const std::string& method) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_handlers.count(method) > 0;
}

JSONRPCResponse MethodDispatcher::Dispatch(const JSONRPCRequest& request) {
    Logger::Debug("Dispatching method: {}", request.method);
    
    try {
        // 濡偓閺屻儲娼堥梽?
        if (!PermissionChecker::Instance().IsMethodAllowed(request.method)) {
            throw PermissionDeniedException("Method not allowed: " + request.method);
        }
        
        // 閹笛嗩攽閺傝纭?
        json result = ExecuteMethod(request.method, request.params);
        
        // 閺嬪嫬缂撻幋鎰閸濆秴绨?
        return ResponseBuilder::CreateSuccessResponse(request.id, result);
        
    } catch (const MCPException& ex) {
        Logger::Error("MCP Exception in {}: {}", request.method, ex.what());
        return ResponseBuilder::CreateErrorResponseFromMCPException(request.id, ex);
        
    } catch (const std::exception& ex) {
        Logger::Error("Exception in {}: {}", request.method, ex.what());
        return ResponseBuilder::CreateErrorResponseFromException(request.id, ex);
    }
}

std::vector<JSONRPCResponse> MethodDispatcher::DispatchBatch(
    const std::vector<JSONRPCRequest>& requests)
{
    std::vector<JSONRPCResponse> responses;
    responses.reserve(requests.size());
    
    Logger::Debug("Dispatching batch of {} requests", requests.size());
    
    for (const auto& request : requests) {
        JSONRPCResponse response = Dispatch(request);
        if (!request.IsNotification()) {
            responses.push_back(response);
        }
    }
    
    return responses;
}

void MethodDispatcher::RegisterDefaultMethods() {
    Logger::Info("Registering default methods...");
    
    // 濞夈劌鍞界化鑽ょ埠閺傝纭?
    RegisterMethod("system.info", [](const json& params) -> json {
        return {
            {"name", "x64dbg MCP Server"},
            {"version", "1.0.4"},
            {"protocol_version", "2.0"},
            {"capabilities", {
                {"debug", true},
                {"memory", true},
                {"registers", true},
                {"breakpoints", true},
                {"disassembly", true},
                {"symbols", true}
            }}
        };
    });
    
    RegisterMethod("system.methods", [this](const json& params) -> json {
        auto methods = GetRegisteredMethods();
        return {{"methods", methods}};
    });
    
    // 濞夈劌鍞?ping 閺傝纭堕敍鍫㈡暏娴滃孩绁寸拠鏇＄箾閹恒儻绱?
    RegisterMethod("system.ping", [](const json& params) -> json {
        return {{"pong", true}};
    });
    
    // 閸忔湹绮弬瑙勭《鐏忓棗婀崥鍕殰閻?Handler 娑擃厽鏁為崘?
    // 娓氬顩? DebugHandler::RegisterMethods(dispatcher);
    
    Logger::Info("Default methods registered");
}

std::vector<std::string> MethodDispatcher::GetRegisteredMethods() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::string> methods;
    methods.reserve(m_handlers.size());
    
    for (const auto& pair : m_handlers) {
        methods.push_back(pair.first);
    }
    
    return methods;
}

json MethodDispatcher::ExecuteMethod(const std::string& method, const json& params) {
    MethodHandler handler;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_handlers.find(method);
        if (it == m_handlers.end()) {
            throw MethodNotFoundException("Method not found: " + method);
        }
        handler = it->second;
    }

    if (!handler) {
        throw MethodNotFoundException("Method not found: " + method);
    }

    try {
        return handler(params);
    } catch (const MCPException&) {
        throw; // rethrow MCP exceptions
    } catch (const json::exception& ex) {
        throw InvalidParamsException("Invalid params: " + std::string(ex.what()));
    } catch (const std::exception& ex) {
        throw MCPException("Error executing method: " + std::string(ex.what()));
    }
}

} // namespace MCP
