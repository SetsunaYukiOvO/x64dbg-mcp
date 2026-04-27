/**
 * @file ModuleHandler.cpp
 * @brief 模块管理方法处理器实现
 */

#include "ModuleHandler.h"
#include "../core/MethodDispatcher.h"
#include "../core/Exceptions.h"
#include "../core/Logger.h"
#include "../utils/StringUtils.h"
#include <_scriptapi_module.h>
#include <bridgelist.h>

namespace MCP {
namespace {

std::string CanonicalText(const std::string& value) {
    return StringUtils::ToLower(StringUtils::FixUtf8Mojibake(value));
}

std::string BaseNameFromPath(const std::string& path) {
    const size_t pos = path.find_last_of("\\/");
    if (pos == std::string::npos) {
        return path;
    }
    return path.substr(pos + 1);
}

std::string StripExtension(const std::string& fileName) {
    const size_t dot = fileName.find_last_of('.');
    if (dot == std::string::npos || dot == 0) {
        return fileName;
    }
    return fileName.substr(0, dot);
}

bool ModuleMatchesQuery(const Script::Module::ModuleInfo& mod, const std::string& query) {
    if (query.empty()) {
        return false;
    }

    const std::string queryLower = CanonicalText(query);
    const std::string name = StringUtils::FixUtf8Mojibake(mod.name);
    const std::string path = StringUtils::FixUtf8Mojibake(mod.path);
    const std::string fileName = BaseNameFromPath(path);

    const std::string nameLower = CanonicalText(name);
    const std::string pathLower = CanonicalText(path);
    const std::string fileLower = CanonicalText(fileName);

    if (queryLower == nameLower || queryLower == pathLower || queryLower == fileLower) {
        return true;
    }

    const bool hasWildcard = queryLower.find('*') != std::string::npos ||
                             queryLower.find('?') != std::string::npos;
    if (hasWildcard) {
        if (StringUtils::WildcardMatchUtf8(queryLower, nameLower) ||
            StringUtils::WildcardMatchUtf8(queryLower, pathLower) ||
            StringUtils::WildcardMatchUtf8(queryLower, fileLower)) {
            return true;
        }
    }

    const std::string queryStemLower = CanonicalText(StripExtension(query));
    if (queryStemLower.empty()) {
        return false;
    }

    const std::string nameStemLower = CanonicalText(StripExtension(name));
    const std::string fileStemLower = CanonicalText(StripExtension(fileName));

    if (hasWildcard) {
        if (StringUtils::WildcardMatchUtf8(queryStemLower, nameStemLower) ||
            StringUtils::WildcardMatchUtf8(queryStemLower, fileStemLower)) {
            return true;
        }
    }

    return queryStemLower == nameStemLower || queryStemLower == fileStemLower;
}

bool ResolveModuleByQueryFallback(const std::string& query, Script::Module::ModuleInfo* outInfo) {
    if (outInfo == nullptr || query.empty()) {
        return false;
    }

    BridgeList<Script::Module::ModuleInfo> moduleList;
    if (!Script::Module::GetList(&moduleList)) {
        return false;
    }

    for (size_t i = 0; i < moduleList.Count(); ++i) {
        const auto& mod = moduleList[i];
        if (ModuleMatchesQuery(mod, query)) {
            *outInfo = mod;
            return true;
        }
    }

    return false;
}

bool ResolveModuleInfo(const std::string& nameOrAddr, Script::Module::ModuleInfo& info) {
    try {
        duint address = StringUtils::ParseAddress(nameOrAddr);
        if (Script::Module::InfoFromAddr(address, &info))
            return true;
    } catch (...) {}
    if (Script::Module::InfoFromName(nameOrAddr.c_str(), &info))
        return true;
    return ResolveModuleByQueryFallback(nameOrAddr, &info);
}

} // namespace

void ModuleHandler::RegisterMethods() {
    auto& dispatcher = MethodDispatcher::Instance();
    
    dispatcher.RegisterMethod("module.list", List);
    dispatcher.RegisterMethod("module.get", Get);
    dispatcher.RegisterMethod("module.get_main", GetMain);
    dispatcher.RegisterMethod("module.get_exports", GetExports);
    dispatcher.RegisterMethod("module.get_imports", GetImports);
    
    Logger::Info("Registered module.* methods");
}

json ModuleHandler::List(const json& params) {
    json result = json::object();
    json modules = json::array();
    
    // 使用 Script API 获取模块列表
    BridgeList<Script::Module::ModuleInfo> moduleList;
    
    if (!Script::Module::GetList(&moduleList)) {
        result["modules"] = modules;
        result["count"] = 0;
        return result;
    }
    
    // 转换为 JSON 数组
    for (size_t i = 0; i < moduleList.Count(); i++) {
        const auto& mod = moduleList[i];
        json module = json::object();
        const std::string fixedName = StringUtils::FixUtf8Mojibake(mod.name);
        const std::string fixedPath = StringUtils::FixUtf8Mojibake(mod.path);
        module["base"] = StringUtils::FormatAddress(mod.base);
        module["size"] = mod.size;
        module["entry"] = StringUtils::FormatAddress(mod.entry);
        module["name"] = fixedName;
        module["path"] = fixedPath;
        module["section_count"] = mod.sectionCount;
        
        modules.push_back(module);
    }
    
    result["modules"] = modules;
    result["count"] = moduleList.Count();
    
    return result;
}

json ModuleHandler::Get(const json& params) {
    if (!params.contains("module") && !params.contains("name") && !params.contains("address")) {
        throw InvalidParamsException("Missing required parameter: module, name or address");
    }
    
    Script::Module::ModuleInfo info = {};
    bool success = false;
    
    if (params.contains("module")) {
        std::string module = params["module"].get<std::string>();
        try {
            duint address = StringUtils::ParseAddress(module);
            success = Script::Module::InfoFromAddr(address, &info);
        } catch (...) {
            success = false;
        }
        if (!success) {
            success = Script::Module::InfoFromName(module.c_str(), &info);
        }
        if (!success) {
            success = ResolveModuleByQueryFallback(module, &info);
        }
    } else if (params.contains("name")) {
        std::string name = params["name"].get<std::string>();
        success = Script::Module::InfoFromName(name.c_str(), &info);
        if (!success) {
            success = ResolveModuleByQueryFallback(name, &info);
        }
    } else {
        std::string addressStr = params["address"].get<std::string>();
        duint address = StringUtils::ParseAddress(addressStr);
        success = Script::Module::InfoFromAddr(address, &info);
    }
    
    if (!success) {
        throw MCPException("Module not found", -32000);
    }
    
    json result = json::object();
    result["base"] = StringUtils::FormatAddress(info.base);
    result["size"] = info.size;
    result["entry"] = StringUtils::FormatAddress(info.entry);
    result["section_count"] = info.sectionCount;
    result["name"] = StringUtils::FixUtf8Mojibake(info.name);
    result["path"] = StringUtils::FixUtf8Mojibake(info.path);
    
    return result;
}

json ModuleHandler::GetMain(const json& params) {
    Script::Module::ModuleInfo info;
    
    if (!Script::Module::GetMainModuleInfo(&info)) {
        throw MCPException("Failed to get main module info", -32000);
    }
    
    json result = json::object();
    result["base"] = StringUtils::FormatAddress(info.base);
    result["size"] = info.size;
    result["entry"] = StringUtils::FormatAddress(info.entry);
    result["section_count"] = info.sectionCount;
    result["name"] = StringUtils::FixUtf8Mojibake(info.name);
    result["path"] = StringUtils::FixUtf8Mojibake(info.path);
    
    return result;
}

json ModuleHandler::GetExports(const json& params) {
    if (!params.contains("module"))
        throw InvalidParamsException("Missing required parameter: module");

    std::string moduleName = params["module"].get<std::string>();
    Script::Module::ModuleInfo modInfo;
    if (!ResolveModuleInfo(moduleName, modInfo))
        throw MCPException("Module not found: " + moduleName);

    BridgeList<Script::Module::ModuleExport> list;
    if (!Script::Module::GetExports(&modInfo, &list))
        throw MCPException("Failed to get exports for: " + moduleName);

    json exports = json::array();
    for (size_t i = 0; i < list.Count(); ++i) {
        const auto& e = list[i];
        json entry;
        entry["name"] = std::string(e.name);
        entry["ordinal"] = static_cast<uint64_t>(e.ordinal);
        entry["rva"] = StringUtils::FormatAddress(static_cast<uint64_t>(e.rva));
        entry["va"] = StringUtils::FormatAddress(static_cast<uint64_t>(e.va));
        if (e.forwarded)
            entry["forward"] = std::string(e.forwardName);
        exports.push_back(entry);
    }

    json result;
    result["module"] = StringUtils::FixUtf8Mojibake(modInfo.name);
    result["count"] = exports.size();
    result["exports"] = exports;
    return result;
}

json ModuleHandler::GetImports(const json& params) {
    if (!params.contains("module"))
        throw InvalidParamsException("Missing required parameter: module");

    std::string moduleName = params["module"].get<std::string>();
    Script::Module::ModuleInfo modInfo;
    if (!ResolveModuleInfo(moduleName, modInfo))
        throw MCPException("Module not found: " + moduleName);

    BridgeList<Script::Module::ModuleImport> list;
    if (!Script::Module::GetImports(&modInfo, &list))
        throw MCPException("Failed to get imports for: " + moduleName);

    json imports = json::array();
    for (size_t i = 0; i < list.Count(); ++i) {
        const auto& imp = list[i];
        json entry;
        entry["name"] = std::string(imp.name);
        entry["ordinal"] = static_cast<uint64_t>(imp.ordinal);
        entry["iat_rva"] = StringUtils::FormatAddress(static_cast<uint64_t>(imp.iatRva));
        entry["iat_va"] = StringUtils::FormatAddress(static_cast<uint64_t>(imp.iatVa));
        imports.push_back(entry);
    }

    json result;
    result["module"] = StringUtils::FixUtf8Mojibake(modInfo.name);
    result["count"] = imports.size();
    result["imports"] = imports;
    return result;
}

} // namespace MCP
