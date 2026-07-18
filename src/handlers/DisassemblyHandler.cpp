#include "DisassemblyHandler.h"
#include "../business/DisassemblyEngine.h"
#include "../business/SymbolResolver.h"
#include "../core/MethodDispatcher.h"
#include "../core/PermissionChecker.h"
#include "../core/RequestValidator.h"
#include "../core/Exceptions.h"
#include "../utils/StringUtils.h"

namespace MCP {

// ==================== DisassemblyHandler ====================

void DisassemblyHandler::RegisterMethods() {
    auto& dispatcher = MethodDispatcher::Instance();
    
    dispatcher.RegisterMethod("disassembly.at", At);
    dispatcher.RegisterMethod("disassembly.range", Range);
    dispatcher.RegisterMethod("disassembly.function", Function);
}

nlohmann::json DisassemblyHandler::At(const nlohmann::json& params) {
    if (!params.contains("address")) {
        throw InvalidParamsException("Missing required parameter: address");
    }
    
    uint64_t address = RequestValidator::GetAddress(params, "address");
    size_t count = params.value("count", 10);
    
    auto& engine = DisassemblyEngine::Instance();
    
    nlohmann::json instrArray = nlohmann::json::array();
    
    if (count == 1) {
        auto instr = engine.DisassembleAt(address);
        instrArray.push_back(InstructionToJson(instr));
    } else {
        auto instructions = engine.DisassembleRange(address, count);
        for (const auto& instr : instructions) {
            instrArray.push_back(InstructionToJson(instr));
        }
    }
    
    nlohmann::json result;
    result["address"] = StringUtils::FormatAddress(address);
    result["count"] = instrArray.size();
    result["instructions"] = instrArray;
    
    return result;
}

nlohmann::json DisassemblyHandler::Range(const nlohmann::json& params) {
    auto& engine = DisassemblyEngine::Instance();

    std::vector<InstructionInfo> instructions;
    uint64_t startAddress = 0;
    uint64_t endAddress = 0;

    // Tool schema mode: {start, end}
    if (params.contains("start") || params.contains("end")) {
        if (!params.contains("start")) {
            throw InvalidParamsException("Missing required parameter: start");
        }
        if (!params.contains("end")) {
            throw InvalidParamsException("Missing required parameter: end");
        }

        startAddress = RequestValidator::GetAddress(params, "start");
        endAddress = RequestValidator::GetAddress(params, "end");
        if (endAddress <= startAddress) {
            throw InvalidParamsException("Invalid disassembly range: end must be greater than start");
        }

        const size_t maxInstructions = params.value("max_instructions", static_cast<size_t>(10000));
        uint64_t current = startAddress;
        while (current < endAddress && instructions.size() < maxInstructions) {
            auto instr = engine.DisassembleAt(current);
            instructions.push_back(instr);
            const uint64_t next = instr.address + instr.size;
            if (next <= current) {
                break;
            }
            current = next;
        }
    } else {
        // Backward compatible mode: {address, count}
        if (!params.contains("address")) {
            throw InvalidParamsException("Missing required parameter: address");
        }
        if (!params.contains("count")) {
            throw InvalidParamsException("Missing required parameter: count");
        }

        startAddress = RequestValidator::GetAddress(params, "address");
        size_t count = params["count"].get<size_t>();
        instructions = engine.DisassembleRange(startAddress, count);
        if (!instructions.empty()) {
            const auto& last = instructions.back();
            endAddress = last.address + last.size;
        } else {
            endAddress = startAddress;
        }
    }

    nlohmann::json instrArray = nlohmann::json::array();
    for (const auto& instr : instructions) {
        instrArray.push_back(InstructionToJson(instr));
    }

    nlohmann::json result;
    result["start"] = StringUtils::FormatAddress(startAddress);
    result["end"] = StringUtils::FormatAddress(endAddress);
    result["count"] = instructions.size();
    result["instructions"] = instrArray;

    return result;
}

nlohmann::json DisassemblyHandler::Function(const nlohmann::json& params) {
    if (!params.contains("address")) {
        throw InvalidParamsException("Missing required parameter: address");
    }
    
    uint64_t address = RequestValidator::GetAddress(params, "address");
    
    auto& engine = DisassemblyEngine::Instance();
    auto& resolver = SymbolResolver::Instance();
    
    // 鑾峰彇鍑芥暟璧峰鍦板潃
    auto funcStart = resolver.GetFunctionStart(address);
    if (!funcStart.has_value()) {
        funcStart = address;
    }
    
    // 鍙嶆眹缂栧嚱鏁?
    auto instructions = engine.DisassembleFunction(funcStart.value());
    
    nlohmann::json instrArray = nlohmann::json::array();
    for (const auto& instr : instructions) {
        instrArray.push_back(InstructionToJson(instr));
    }
    
    nlohmann::json result;
    result["start"] = StringUtils::FormatAddress(funcStart.value());
    result["count"] = instructions.size();
    result["instructions"] = instrArray;
    
    // 濡傛灉鍙互鑾峰彇鍑芥暟缁撴潫鍦板潃
    if (!instructions.empty()) {
        auto lastInstr = instructions.back();
        result["end"] = StringUtils::FormatAddress(lastInstr.address + lastInstr.size);
    }
    
    return result;
}

nlohmann::json DisassemblyHandler::InstructionToJson(const InstructionInfo& instr) {
    nlohmann::json json;
    
    json["address"] = StringUtils::FormatAddress(instr.address);
    json["bytes"] = StringUtils::BytesToHex(instr.bytes);
    json["mnemonic"] = instr.mnemonic;
    json["operands"] = instr.operands;
    json["instruction"] = instr.full;
    json["size"] = instr.size;
    
    if (!instr.comment.empty()) {
        json["comment"] = instr.comment;
    }
    
    if (instr.isBranch) {
        json["is_branch"] = true;
    }
    
    if (instr.isCall) {
        json["is_call"] = true;
    }
    
    if (instr.isRet) {
        json["is_ret"] = true;
    }
    
    if (instr.branchTarget.has_value()) {
        json["branch_target"] = StringUtils::FormatAddress(instr.branchTarget.value());
    }
    
    return json;
}

// ==================== SymbolHandler ====================

void SymbolHandler::RegisterMethods() {
    auto& dispatcher = MethodDispatcher::Instance();
    
    dispatcher.RegisterMethod("symbol.resolve", Resolve);
    dispatcher.RegisterMethod("symbol.from_address", FromAddress);
    dispatcher.RegisterMethod("symbol.search", Search);
    dispatcher.RegisterMethod("symbol.list", List);
    dispatcher.RegisterMethod("symbol.modules", Modules);
    dispatcher.RegisterMethod("symbol.set_label", SetLabel);
    dispatcher.RegisterMethod("symbol.set_comment", SetComment);
    dispatcher.RegisterMethod("symbol.get_comment", GetComment);
}

nlohmann::json SymbolHandler::Resolve(const nlohmann::json& params) {
    if (!params.contains("symbol")) {
        throw InvalidParamsException("Missing required parameter: symbol");
    }
    
    std::string symbol = params["symbol"].get<std::string>();
    
    auto& resolver = SymbolResolver::Instance();
    auto address = resolver.GetAddressFromSymbol(symbol);
    
    if (!address.has_value()) {
        throw InvalidParamsException("Symbol not found: " + symbol);
    }
    
    nlohmann::json result;
    result["symbol"] = symbol;
    result["address"] = StringUtils::FormatAddress(address.value());
    
    // 鑾峰彇璇︾粏淇℃伅
    auto info = resolver.GetSymbolInfoFromAddress(address.value());
    if (info.has_value()) {
        result["module"] = info->module;
        result["type"] = resolver.SymbolTypeToString(info->type);
    }
    
    return result;
}

nlohmann::json SymbolHandler::FromAddress(const nlohmann::json& params) {
    if (!params.contains("address")) {
        throw InvalidParamsException("Missing required parameter: address");
    }
    
    uint64_t address = RequestValidator::GetAddress(params, "address");
    bool includeOffset = params.value("include_offset", true);
    
    auto& resolver = SymbolResolver::Instance();
    auto symbol = resolver.GetSymbolFromAddress(address, includeOffset);
    
    nlohmann::json result;
    result["address"] = StringUtils::FormatAddress(address);
    
    if (symbol.has_value()) {
        result["symbol"] = symbol.value();
    } else {
        result["symbol"] = nullptr;
    }
    
    return result;
}

nlohmann::json SymbolHandler::Search(const nlohmann::json& params) {
    if (!params.contains("pattern")) {
        throw InvalidParamsException("Missing required parameter: pattern");
    }
    
    std::string pattern = params["pattern"].get<std::string>();
    
    auto& resolver = SymbolResolver::Instance();
    auto symbols = resolver.SearchSymbols(pattern);
    
    nlohmann::json symbolArray = nlohmann::json::array();
    for (const auto& symbol : symbols) {
        symbolArray.push_back(SymbolInfoToJson(symbol));
    }
    
    nlohmann::json result;
    result["pattern"] = pattern;
    result["count"] = symbols.size();
    result["symbols"] = symbolArray;
    
    return result;
}

nlohmann::json SymbolHandler::List(const nlohmann::json& params) {
    auto& resolver = SymbolResolver::Instance();
    std::string moduleName;
    if (params.contains("module") && !params["module"].is_null()) {
        moduleName = params["module"].get<std::string>();
    } else {
        auto mainModule = resolver.GetModuleInfo("");
        if (!mainModule.has_value()) {
            throw MCPException("Failed to resolve main module", -32000);
        }
        moduleName = mainModule->name;
    }

    auto symbols = resolver.EnumerateSymbols(moduleName);
    
    nlohmann::json symbolArray = nlohmann::json::array();
    for (const auto& symbol : symbols) {
        symbolArray.push_back(SymbolInfoToJson(symbol));
    }
    
    nlohmann::json result;
    result["module"] = moduleName;
    result["count"] = symbols.size();
    result["symbols"] = symbolArray;
    
    return result;
}

nlohmann::json SymbolHandler::Modules(const nlohmann::json& params) {
    auto& resolver = SymbolResolver::Instance();
    auto modules = resolver.EnumerateModules();
    
    nlohmann::json moduleArray = nlohmann::json::array();
    for (const auto& module : modules) {
        moduleArray.push_back(ModuleInfoToJson(module));
    }
    
    nlohmann::json result;
    result["count"] = modules.size();
    result["modules"] = moduleArray;
    
    return result;
}

nlohmann::json SymbolHandler::SetLabel(const nlohmann::json& params) {
    // 妫€鏌ュ啓鏉冮檺
    if (!PermissionChecker::Instance().IsMemoryWriteAllowed()) {
        throw PermissionDeniedException("Setting label requires write permission");
    }
    
    if (!params.contains("address")) {
        throw InvalidParamsException("Missing required parameter: address");
    }
    if (!params.contains("label")) {
        throw InvalidParamsException("Missing required parameter: label");
    }
    
    uint64_t address = RequestValidator::GetAddress(params, "address");
    std::string label = params["label"].get<std::string>();
    
    auto& resolver = SymbolResolver::Instance();
    bool success = resolver.SetLabel(address, label);
    
    if (!success) {
        throw MCPException("Failed to set label at: " +
                           StringUtils::FormatAddress(address));
    }
    
    nlohmann::json result;
    result["success"] = true;
    result["address"] = StringUtils::FormatAddress(address);
    result["label"] = label;
    
    return result;
}

nlohmann::json SymbolHandler::SetComment(const nlohmann::json& params) {
    // 妫€鏌ュ啓鏉冮檺
    if (!PermissionChecker::Instance().IsMemoryWriteAllowed()) {
        throw PermissionDeniedException("Setting comment requires write permission");
    }
    
    if (!params.contains("address")) {
        throw InvalidParamsException("Missing required parameter: address");
    }
    if (!params.contains("comment")) {
        throw InvalidParamsException("Missing required parameter: comment");
    }
    
    uint64_t address = RequestValidator::GetAddress(params, "address");
    std::string comment = params["comment"].get<std::string>();
    
    auto& resolver = SymbolResolver::Instance();
    bool success = resolver.SetComment(address, comment);
    
    if (!success) {
        throw MCPException("Failed to set comment at: " +
                           StringUtils::FormatAddress(address));
    }
    
    nlohmann::json result;
    result["success"] = true;
    result["address"] = StringUtils::FormatAddress(address);
    result["comment"] = comment;
    
    return result;
}

nlohmann::json SymbolHandler::GetComment(const nlohmann::json& params) {
    if (!params.contains("address")) {
        throw InvalidParamsException("Missing required parameter: address");
    }
    
    uint64_t address = RequestValidator::GetAddress(params, "address");
    
    auto& resolver = SymbolResolver::Instance();
    auto comment = resolver.GetComment(address);
    
    nlohmann::json result;
    result["address"] = StringUtils::FormatAddress(address);
    
    if (comment.has_value()) {
        result["comment"] = comment.value();
    } else {
        result["comment"] = nullptr;
    }
    
    return result;
}

nlohmann::json SymbolHandler::SymbolInfoToJson(const SymbolInfo& symbol) {
    nlohmann::json json;
    
    json["name"] = symbol.name;
    json["address"] = StringUtils::FormatAddress(symbol.address);
    json["module"] = symbol.module;
    
    // 绗﹀彿绫诲瀷
    switch (symbol.type) {
        case SymbolType::Function:
            json["type"] = "function";
            break;
        case SymbolType::Export:
            json["type"] = "export";
            break;
        case SymbolType::Import:
            json["type"] = "import";
            break;
        case SymbolType::Label:
            json["type"] = "label";
            break;
        case SymbolType::Data:
            json["type"] = "data";
            break;
    }
    
    if (symbol.size > 0) {
        json["size"] = symbol.size;
    }
    
    if (!symbol.decorated.empty()) {
        json["decorated"] = symbol.decorated;
    }
    
    return json;
}

nlohmann::json SymbolHandler::ModuleInfoToJson(const ModuleInfo& module) {
    nlohmann::json json;
    
    json["name"] = module.name;
    json["path"] = module.path;
    json["base"] = StringUtils::FormatAddress(module.base);
    json["size"] = module.size;
    json["entry"] = StringUtils::FormatAddress(module.entry);
    json["is_system"] = module.isSystemModule;
    
    return json;
}

} // namespace MCP

