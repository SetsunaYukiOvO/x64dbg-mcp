#include "AnalysisHandler.h"
#include "../core/MethodDispatcher.h"
#include "../core/Exceptions.h"
#include "../core/Logger.h"
#include "../core/X64DBGBridge.h"
#include "../utils/StringUtils.h"
#include "../business/DebugController.h"
#include <_scriptapi_function.h>
#include <bridgelist.h>

namespace MCP {

void AnalysisHandler::RegisterMethods() {
    auto& d = MethodDispatcher::Instance();
    d.RegisterMethod("xref.get", XrefGet);
    d.RegisterMethod("function.list", FunctionList);
    d.RegisterMethod("function.get", FunctionGet);
}

nlohmann::json AnalysisHandler::XrefGet(const nlohmann::json& params) {
    if (!params.contains("address"))
        throw InvalidParamsException("Missing required parameter: address");

    uint64_t addr = StringUtils::ParseAddress(params["address"].get<std::string>());

    XREF_INFO info;
    memset(&info, 0, sizeof(info));
    bool found = DbgXrefGet(static_cast<duint>(addr), &info);

    nlohmann::json refs = nlohmann::json::array();
    if (found && info.refcount > 0) {
        for (duint i = 0; i < info.refcount; ++i) {
            nlohmann::json entry;
            entry["address"] = StringUtils::FormatAddress(static_cast<uint64_t>(info.references[i].addr));
            switch (info.references[i].type) {
                case XREF_CALL: entry["type"] = "call"; break;
                case XREF_JMP:  entry["type"] = "jmp";  break;
                case XREF_DATA: entry["type"] = "data"; break;
                default:        entry["type"] = "unknown"; break;
            }
            refs.push_back(entry);
        }
        if (info.references)
            BridgeFree(info.references);
    }

    nlohmann::json result;
    result["address"] = StringUtils::FormatAddress(addr);
    result["count"] = refs.size();
    result["references"] = refs;
    return result;
}

nlohmann::json AnalysisHandler::FunctionList(const nlohmann::json& params) {
    if (!DebugController::Instance().IsDebugging())
        throw DebuggerNotRunningException();

    BridgeList<Script::Function::FunctionInfo> list;
    if (!Script::Function::GetList(&list))
        throw MCPException("Failed to get function list");

    std::string filterModule;
    if (params.contains("module") && !params["module"].is_null())
        filterModule = params["module"].get<std::string>();

    nlohmann::json funcs = nlohmann::json::array();
    for (size_t i = 0; i < list.Count(); ++i) {
        const auto& f = list[i];
        if (!filterModule.empty()) {
            std::string mod(f.mod);
            if (mod.find(filterModule) == std::string::npos)
                continue;
        }
        nlohmann::json entry;
        entry["module"] = std::string(f.mod);
        entry["start_rva"] = StringUtils::FormatAddress(static_cast<uint64_t>(f.rvaStart));
        entry["end_rva"] = StringUtils::FormatAddress(static_cast<uint64_t>(f.rvaEnd));
        entry["instruction_count"] = static_cast<uint64_t>(f.instructioncount);
        entry["manual"] = f.manual;
        funcs.push_back(entry);
    }

    nlohmann::json result;
    result["count"] = funcs.size();
    result["functions"] = funcs;
    return result;
}

nlohmann::json AnalysisHandler::FunctionGet(const nlohmann::json& params) {
    if (!params.contains("address"))
        throw InvalidParamsException("Missing required parameter: address");

    uint64_t addr = StringUtils::ParseAddress(params["address"].get<std::string>());

    duint start = 0, end = 0, icount = 0;
    bool found = Script::Function::Get(static_cast<duint>(addr), &start, &end, &icount);

    nlohmann::json result;
    result["found"] = found;
    result["address"] = StringUtils::FormatAddress(addr);
    if (found) {
        result["start"] = StringUtils::FormatAddress(static_cast<uint64_t>(start));
        result["end"] = StringUtils::FormatAddress(static_cast<uint64_t>(end));
        result["size"] = static_cast<uint64_t>(end - start);
        result["instruction_count"] = static_cast<uint64_t>(icount);
    }
    return result;
}

} // namespace MCP
