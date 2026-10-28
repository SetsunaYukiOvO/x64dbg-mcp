#include "SymbolResolver.h"
#include "DebugController.h"
#include "../core/Logger.h"
#include "../core/Exceptions.h"
#include "../core/X64DBGBridge.h"
#include <cstdio>

namespace MCP {

std::optional<std::string> SymbolResolver::GetSymbolFromAddress(uint64_t address, bool includeOffset) {
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    SYMBOLINFOCPP symInfo;
    
    if (!DbgGetSymbolInfoAt(address, &symInfo)) {
        // 尝试获取模块+偏移
        auto moduleInfo = GetModuleFromAddress(address);
        if (moduleInfo.has_value()) {
            uint64_t offset = address - moduleInfo->base;
            char offsetStr[32];
            snprintf(offsetStr, sizeof(offsetStr), "+0x%llX", (unsigned long long)offset);
            return moduleInfo->name + offsetStr;
        }
        return std::nullopt;
    }
    
    std::string result = symInfo.undecoratedSymbol ? symInfo.undecoratedSymbol : 
                        (symInfo.decoratedSymbol ? symInfo.decoratedSymbol : "");
    
    // 如果需要偏移量,计算符号偏移
    if (includeOffset) {
        auto symbolAddr = GetAddressFromSymbol(result);
        if (symbolAddr.has_value() && symbolAddr.value() != address) {
            uint64_t offset = address - symbolAddr.value();
            char offsetStr[32];
            snprintf(offsetStr, sizeof(offsetStr), "+0x%llX", (unsigned long long)offset);
            result += offsetStr;
        }
    }
    
    Logger::Trace("Resolved symbol at 0x{:X}: {}", address, result);
    return result;
}


std::optional<SymbolInfo> SymbolResolver::GetSymbolInfoFromAddress(uint64_t address) {
    if (!DebugController::Instance().IsDebugging()) {
        throw DebuggerNotRunningException();
    }
    
    SYMBOLINFOCPP symInfo;
    
    if (!DbgGetSymbolInfoAt(address, &symInfo)) {
        return std::nullopt;
    }
    
    SymbolInfo info;
    info.name = symInfo.undecoratedSymbol ? symInfo.undecoratedSymbol : "";
    info.address = symInfo.addr;
    info.module = "";  // SYMBOLINFO doesn't have module
    info.size = 0;  // SYMBOLINFO doesn't have size
    info.decorated = symInfo.decoratedSymbol ? symInfo.decoratedSymbol : "";
    
    // 确定符号类型
    switch (symInfo.type) {
        case SYMBOLTYPE::sym_import:
            info.type = SymbolType::Import;
            break;
        case SYMBOLTYPE::sym_export:
            info.type = SymbolType::Export;
            break;
        default:
            info.type = SymbolType::Function;
            break;
    }
    
    return info;
}


} // namespace MCP
