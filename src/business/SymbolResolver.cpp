#include "SymbolResolver.h"
#include "DebugController.h"
#include "../core/Logger.h"
#include "../core/Exceptions.h"
#include "../utils/StringUtils.h"
#include "../core/X64DBGBridge.h"
#include <algorithm>
#include <cstdio>

#ifdef XDBG_SDK_AVAILABLE
#include "_scriptapi_function.h"
#include "_scriptapi_label.h"
#include "_scriptapi_module.h"
#include "_scriptapi_symbol.h"
#include <bridgelist.h>
#endif

namespace MCP {

SymbolResolver& SymbolResolver::Instance() {
    static SymbolResolver instance;
    return instance;
}


std::optional<uint64_t> SymbolResolver::GetAddressFromSymbol(const std::string& symbol) {
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    bool success = false;
    uint64_t address = DbgEval(symbol.c_str(), &success);
    
    if (!success) {
        Logger::Warning("Symbol not found: {}", symbol);
        return std::nullopt;
    }
    
    Logger::Trace("Resolved symbol '{}' to 0x{:X}", symbol, address);
    return address;
}

std::optional<SymbolInfo> SymbolResolver::GetSymbolInfo(const std::string& symbol) {
    auto address = GetAddressFromSymbol(symbol);
    if (!address.has_value()) {
        return std::nullopt;
    }
    
    return GetSymbolInfoFromAddress(address.value());
}


std::vector<SymbolInfo> SymbolResolver::EnumerateSymbols(const std::string& moduleName,
                                                         std::optional<SymbolType> typeFilter)
{
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    std::vector<SymbolInfo> symbols;

#ifdef XDBG_SDK_AVAILABLE
    auto moduleInfo = GetModuleInfo(moduleName);
    if (!moduleInfo.has_value()) {
        Logger::Warning("Failed to resolve module for symbol enumeration: '{}'", moduleName);
        return symbols;
    }

    const std::string resolvedModuleName = moduleInfo->name;
    const uint64_t moduleBase = moduleInfo->base;
    const std::string targetLower = StringUtils::ToLower(resolvedModuleName);

    BridgeList<Script::Symbol::SymbolInfo> symbolList;
    if (!Script::Symbol::GetList(&symbolList)) {
        Logger::Warning("Script::Symbol::GetList failed");
        return symbols;
    }

    symbols.reserve(symbolList.Count());
    for (size_t i = 0; i < symbolList.Count(); ++i) {
        const auto& sym = symbolList[i];
        if (StringUtils::ToLower(sym.mod) != targetLower) {
            continue;
        }

        SymbolInfo info;
        info.name = sym.name;
        info.module = sym.mod;
        info.address = moduleBase + sym.rva;
        info.size = 0;
        info.decorated = "";

        switch (sym.type) {
            case Script::Symbol::Function:
                info.type = SymbolType::Function;
                break;
            case Script::Symbol::Import:
                info.type = SymbolType::Import;
                break;
            case Script::Symbol::Export:
                info.type = SymbolType::Export;
                break;
            default:
                info.type = SymbolType::Function;
                break;
        }

        if (typeFilter.has_value() && info.type != typeFilter.value()) {
            continue;
        }

        symbols.push_back(std::move(info));
    }
#endif

    // 使用 x64dbg API 枚举符号
    // 实际实现需要调用 DbgEnumSymbols 或类似函数
    
    Logger::Debug("Enumerated {} symbols from module '{}'", symbols.size(), moduleName.empty() ? "<main>" : moduleName);
    return symbols;
}

std::vector<SymbolInfo> SymbolResolver::SearchSymbols(const std::string& pattern) {
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    std::vector<SymbolInfo> results;
    
    // 枚举所有模块的符号并匹配模式
    auto modules = EnumerateModules();
    
    for (const auto& module : modules) {
        auto symbols = EnumerateSymbols(module.name);
        
        for (const auto& symbol : symbols) {
            if (MatchPattern(symbol.name, pattern)) {
                results.push_back(symbol);
            }
        }
    }
    
    Logger::Debug("Found {} symbols matching pattern '{}'", results.size(), pattern);
    return results;
}

std::optional<ModuleInfo> SymbolResolver::GetModuleInfo(const std::string& moduleName) {
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    // 枚举所有模块并查找匹配的
    #ifdef XDBG_SDK_AVAILABLE
    if (moduleName.empty()) {
        Script::Module::ModuleInfo info{};
        if (Script::Module::GetMainModuleInfo(&info)) {
            ModuleInfo module;
            module.name = info.name;
            module.path = info.path;
            module.base = info.base;
            module.size = info.size;
            module.entry = info.entry;
            const std::string lowerPath = StringUtils::ToLower(module.path);
            module.isSystemModule = lowerPath.find("\\windows\\") != std::string::npos ||
                                    lowerPath.find("\\system32\\") != std::string::npos;
            return module;
        }
        return std::nullopt;
    }

    Script::Module::ModuleInfo info{};
    if (Script::Module::InfoFromName(moduleName.c_str(), &info)) {
        ModuleInfo module;
        module.name = info.name;
        module.path = info.path;
        module.base = info.base;
        module.size = info.size;
        module.entry = info.entry;
        const std::string lowerPath = StringUtils::ToLower(module.path);
        module.isSystemModule = lowerPath.find("\\windows\\") != std::string::npos ||
                                lowerPath.find("\\system32\\") != std::string::npos;
        return module;
    }
    #endif

    auto modules = EnumerateModules();
    const std::string normalizedName = StringUtils::ToLower(moduleName);
    for (const auto& module : modules) {
        if (StringUtils::ToLower(module.name) == normalizedName) {
            return module;
        }
    }
    
    return std::nullopt;
}

std::optional<ModuleInfo> SymbolResolver::GetModuleFromAddress(uint64_t address) {
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    // 使用 x64dbg API 获取模块
    #ifdef XDBG_SDK_AVAILABLE
    Script::Module::ModuleInfo info{};
    if (Script::Module::InfoFromAddr(address, &info)) {
        ModuleInfo module;
        module.name = info.name;
        module.path = info.path;
        module.base = info.base;
        module.size = info.size;
        module.entry = info.entry;
        const std::string lowerPath = StringUtils::ToLower(module.path);
        module.isSystemModule = lowerPath.find("\\windows\\") != std::string::npos ||
                                lowerPath.find("\\system32\\") != std::string::npos;
        return module;
    }
    #endif

    char moduleName[MAX_MODULE_SIZE] = {0};
    if (!DbgGetModuleAt(address, moduleName)) {
        return std::nullopt;
    }
    return GetModuleInfo(moduleName);
}

std::vector<ModuleInfo> SymbolResolver::EnumerateModules() {
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    std::vector<ModuleInfo> modules;

#ifdef XDBG_SDK_AVAILABLE
    BridgeList<Script::Module::ModuleInfo> moduleList;
    if (!Script::Module::GetList(&moduleList)) {
        Logger::Warning("Script::Module::GetList failed");
        return modules;
    }

    modules.reserve(moduleList.Count());
    for (size_t i = 0; i < moduleList.Count(); ++i) {
        const auto& mod = moduleList[i];
        ModuleInfo info;
        info.name = mod.name;
        info.path = mod.path;
        info.base = mod.base;
        info.size = mod.size;
        info.entry = mod.entry;
        const std::string lowerPath = StringUtils::ToLower(info.path);
        info.isSystemModule = lowerPath.find("\\windows\\") != std::string::npos ||
                              lowerPath.find("\\system32\\") != std::string::npos;
        modules.push_back(std::move(info));
    }
#endif

    // 使用 x64dbg API 枚举模块
    // 实际实现需要调用 DbgEnumModules 或类似函数
    
    Logger::Debug("Enumerated {} modules", modules.size());
    return modules;
}

std::optional<uint64_t> SymbolResolver::GetFunctionStart(uint64_t address) {
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
#ifdef XDBG_SDK_AVAILABLE
    // 使用Script::Function::GetInfo来获取函数信息
    Script::Function::FunctionInfo info = {};
    duint start = 0;
    if (Script::Function::Get(address, &start, nullptr, nullptr)) {
        return start;
    }
#else
    uint64_t start = DbgGetFunctionStart(address);
    
    if (start == 0) {
        return std::nullopt;
    }
    
    return start;
#endif
    return std::nullopt;
}

std::optional<uint64_t> SymbolResolver::GetFunctionEnd(uint64_t address) {
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
#ifdef XDBG_SDK_AVAILABLE
    // 使用Script::Function::GetInfo来获取函数信息
    Script::Function::FunctionInfo info = {};
    duint end = 0;
    if (Script::Function::Get(address, nullptr, &end, nullptr)) {
        return end;
    }
#else
    uint64_t end = DbgGetFunctionEnd(address);
    
    if (end == 0) {
        return std::nullopt;
    }
    
    return end;
#endif
    return std::nullopt;
}

bool SymbolResolver::SetLabel(uint64_t address, const std::string& label) {
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    if (!DbgSetLabelAt(address, label.c_str())) {
        Logger::Error("Failed to set label at 0x{:X}", address);
        return false;
    }
    
    Logger::Debug("Set label at 0x{:X}: {}", address, label);
    return true;
}

bool SymbolResolver::DeleteLabel(uint64_t address) {
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    // 设置空标签即删除
    if (!DbgSetLabelAt(address, "")) {
        return false;
    }
    
    Logger::Debug("Deleted label at 0x{:X}", address);
    return true;
}

bool SymbolResolver::SetComment(uint64_t address, const std::string& comment) {
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    if (!DbgSetCommentAt(address, comment.c_str())) {
        Logger::Error("Failed to set comment at 0x{:X}", address);
        return false;
    }
    
    Logger::Debug("Set comment at 0x{:X}: {}", address, comment);
    return true;
}

std::optional<std::string> SymbolResolver::GetComment(uint64_t address) {
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    char comment[4096] = {0};
    
    if (!DbgGetCommentAt(address, comment)) {
        return std::nullopt;
    }
    
    if (comment[0] == '\0') {
        return std::nullopt;
    }
    
    return std::string(comment);
}

std::string SymbolResolver::SymbolTypeToString(SymbolType type) {
    switch (type) {
        case SymbolType::Function:
            return "Function";
        case SymbolType::Export:
            return "Export";
        case SymbolType::Import:
            return "Import";
        case SymbolType::Label:
            return "Label";
        case SymbolType::Data:
            return "Data";
        default:
            return "Unknown";
    }
}

bool SymbolResolver::MatchPattern(const std::string& text, const std::string& pattern) {
    // 简单的通配符匹配 (支持 * 通配符)
    if (pattern.empty()) {
        return text.empty();
    }
    
    if (pattern == "*") {
        return true;
    }
    
    // 转换为小写进行不区分大小写的匹配
    std::string lowerText = StringUtils::ToLower(text);
    std::string lowerPattern = StringUtils::ToLower(pattern);
    
    // 简单实现: 检查是否包含 (不支持完整的通配符语法)
    if (lowerPattern.find('*') != std::string::npos) {
        // 去除通配符后检查包含
        std::string cleaned = lowerPattern;
        cleaned.erase(std::remove(cleaned.begin(), cleaned.end(), '*'), cleaned.end());
        return lowerText.find(cleaned) != std::string::npos;
    }
    
    return lowerText == lowerPattern;
}

} // namespace MCP
