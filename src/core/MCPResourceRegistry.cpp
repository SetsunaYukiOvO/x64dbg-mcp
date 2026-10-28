/**
 * @file MCPResourceRegistry.cpp
 * @brief MCP Resource Registry Implementation
 */

#include "MCPResourceRegistry.h"
#include "Logger.h"
#include "MethodDispatcher.h"
#include "JSONRPCParser.h"
#include "RequestValidator.h"
#include "UInt32Validation.h"
#include "Exceptions.h"
#include <nlohmann/json.hpp>
#include <sstream>

namespace MCP {

json MCPResourceDefinition::ToMCPFormat() const {
    return json{
        {"uri", uri},
        {"name", name},
        {"description", description},
        {"mimeType", mimeType}
    };
}

json MCPResourceTemplate::ToMCPFormat() const {
    return json{
        {"uriTemplate", uriTemplate},
        {"name", name},
        {"description", description},
        {"mimeType", mimeType}
    };
}

json MCPResourceContent::ToMCPFormat() const {
    json content;
    content["uri"] = uri;
    content["mimeType"] = mimeType;
    
    if (!text.empty()) {
        content["text"] = text;
    }
    if (!blob.empty()) {
        content["blob"] = blob;
    }
    
    return content;
}

MCPResourceRegistry& MCPResourceRegistry::Instance() {
    static MCPResourceRegistry instance;
    return instance;
}

void MCPResourceRegistry::RegisterResource(const MCPResourceDefinition& resource) {
    m_resources.push_back(resource);
}

void MCPResourceRegistry::RegisterTemplate(const MCPResourceTemplate& template_def) {
    m_templates.push_back(template_def);
}

void MCPResourceRegistry::RegisterDefaultResources() {
    Logger::Info("Registering default MCP resources...");
    
    // 1. Debugger State Resources (Direct Resources)
    RegisterResource({
        "debugger://state/current",
        "debugger-state",
        "Current Debugger State",
        "Current debugging state including running status, process info, and execution context",
        "application/json"
    });
    
    RegisterResource({
        "debugger://registers/all",
        "registers-all",
        "All Registers",
        "Complete snapshot of all CPU registers (general purpose, flags, segments, etc.)",
        "application/json"
    });
    
    RegisterResource({
        "debugger://modules/list",
        "modules-list",
        "Loaded Modules",
        "List of all loaded modules/DLLs with base addresses and sizes",
        "application/json"
    });
    
    RegisterResource({
        "debugger://threads/list",
        "threads-list",
        "Active Threads",
        "List of all threads in the debugged process",
        "application/json"
    });
    
    RegisterResource({
        "debugger://memory/map",
        "memory-map",
        "Memory Map",
        "Complete memory map showing all allocated regions and their protections",
        "application/json"
    });
    
    RegisterResource({
        "debugger://breakpoints/all",
        "breakpoints-all",
        "All Breakpoints",
        "List of all breakpoints (software, hardware, memory) with their conditions",
        "application/json"
    });
    
    RegisterResource({
        "debugger://stack/trace",
        "stack-trace",
        "Stack Trace",
        "Current call stack with return addresses and frame information",
        "application/json"
    });
    
    // 2. Resource Templates (Dynamic Resources with Parameters)
    RegisterTemplate({
        "memory://{address}/{size}",
        "memory-read",
        "Memory Content",
        "Read memory content at specified address and size (hex format)",
        "application/json"
    });
    
    RegisterTemplate({
        "disassembly://{address}/{count}",
        "disassembly-at",
        "Disassembly Listing",
        "Disassemble instructions starting at address for count instructions",
        "application/json"
    });
    
    RegisterTemplate({
        "module://{module_name}/info",
        "module-info",
        "Module Information",
        "Detailed information about a specific module",
        "application/json"
    });
    
    RegisterTemplate({
        "module://{module_name}/exports",
        "module-exports",
        "Module Exports",
        "Exported functions from a specific module",
        "application/json"
    });
    
    RegisterTemplate({
        "module://{module_name}/imports",
        "module-imports",
        "Module Imports",
        "Imported functions used by a specific module",
        "application/json"
    });
    
    RegisterTemplate({
        "symbol://{name}/resolve",
        "symbol-resolve",
        "Symbol Resolution",
        "Resolve symbol name to address",
        "application/json"
    });
    
    RegisterTemplate({
        "function://{address}/disassembly",
        "function-disassembly",
        "Function Disassembly",
        "Complete disassembly of a function at given address",
        "application/json"
    });
    
    RegisterTemplate({
        "breakpoint://{address}/info",
        "breakpoint-info",
        "Breakpoint Details",
        "Detailed information about a breakpoint at specific address",
        "application/json"
    });
    
    Logger::Info("Registered {} resources and {} templates", 
                 m_resources.size(), m_templates.size());
}

std::optional<MCPResourceDefinition> MCPResourceRegistry::FindResource(const std::string& uri) const {
    for (const auto& res : m_resources) {
        if (res.uri == uri) {
            return res;
        }
    }
    return std::nullopt;
}

std::optional<MCPResourceTemplate> MCPResourceRegistry::FindTemplate(const std::string& uriOrName) const {
    for (const auto& tmpl : m_templates) {
        if (tmpl.name == uriOrName || tmpl.uriTemplate == uriOrName) {
            return tmpl;
        }
    }
    return std::nullopt;
}

json MCPResourceRegistry::GenerateResourcesListResponse() const {
    json resources_array = json::array();
    
    for (const auto& resource : m_resources) {
        resources_array.push_back(resource.ToMCPFormat());
    }
    
    return json{
        {"resources", resources_array}
    };
}

json MCPResourceRegistry::GenerateTemplatesListResponse() const {
    json templates_array = json::array();
    
    for (const auto& tmpl : m_templates) {
        templates_array.push_back(tmpl.ToMCPFormat());
    }
    
    return json{
        {"resourceTemplates", templates_array}
    };
}

MCPResourceContent MCPResourceRegistry::ReadResource(const std::string& uri) const {
    std::string method;
    json params = json::object();
    if (uri == "debugger://state/current") method = "debug.get_state";
    else if (uri == "debugger://registers/all") method = "register.list";
    else if (uri == "debugger://modules/list") method = "module.list";
    else if (uri == "debugger://threads/list") method = "thread.list";
    else if (uri == "debugger://memory/map") method = "memory.enumerate";
    else if (uri == "debugger://breakpoints/all") method = "breakpoint.list";
    else if (uri == "debugger://stack/trace") method = "stack.get_trace";
    else {
        const size_t schemeEnd = uri.find("://");
        const size_t slash = schemeEnd == std::string::npos ? std::string::npos : uri.find('/', schemeEnd + 3);
        if (slash == std::string::npos || uri.find_first_of("?#") != std::string::npos || uri.find('/', slash + 1) != std::string::npos)
            throw InvalidParamsException("Invalid resource URI");
        const std::string scheme = uri.substr(0, schemeEnd);
        const auto decode = [](const std::string& part) {
            std::string value;
            const auto hex = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            for (size_t i = 0; i < part.size(); ++i) {
                unsigned char c = part[i];
                if (c == '%') {
                    if (i + 2 >= part.size() || hex(part[i + 1]) < 0 || hex(part[i + 2]) < 0)
                        throw InvalidParamsException("Invalid URI escape");
                    c = static_cast<unsigned char>((hex(part[i + 1]) << 4) | hex(part[i + 2]));
                    i += 2;
                }
                if (c < 32 || c == 127) throw InvalidParamsException("Invalid URI character");
                value += static_cast<char>(c);
            }
            if (value.empty()) throw InvalidParamsException("Empty resource parameter");
            return value;
        };
        const std::string value = decode(uri.substr(schemeEnd + 3, slash - schemeEnd - 3));
        const std::string operation = decode(uri.substr(slash + 1));
        if (scheme == "memory" || scheme == "disassembly") {
            params["address"] = RequestValidator::ValidateAddress(value);
            const uint32_t count = ValidateUInt32(operation, "resource size/count");
            if (count > (scheme == "memory" ? 1048576U : 10000U)) throw InvalidParamsException("Resource request too large");
            params[scheme == "memory" ? "size" : "count"] = count;
            method = scheme == "memory" ? "memory.read" : "disassembly.at";
        } else if (scheme == "module") {
            params["module"] = value;
            if (operation == "info") method = "module.get";
            else if (operation == "exports") method = "module.get_exports";
            else if (operation == "imports") method = "module.get_imports";
        } else if (scheme == "symbol" && operation == "resolve") {
            params["symbol"] = value;
            method = "symbol.resolve";
        } else if (scheme == "function" && operation == "disassembly") {
            params["address"] = RequestValidator::ValidateAddress(value);
            method = "disassembly.function";
        } else if (scheme == "breakpoint" && operation == "info") {
            params["address"] = RequestValidator::ValidateAddress(value);
            method = "breakpoint.get";
        }
        if (method.empty()) throw ResourceNotFoundException("Unknown resource URI: " + uri);
    }
    JSONRPCRequest request{"2.0", int64_t(1), method, params};
    const auto response = MethodDispatcher::Instance().Dispatch(request);
    if (response.error) throw MCPException(response.error->message, response.error->code);
    return MCPResourceContent{uri, "application/json", response.result.dump(2), ""};
}

} // namespace MCP
