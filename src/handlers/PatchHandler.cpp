#include "PatchHandler.h"
#include "../core/MethodDispatcher.h"
#include "../core/Exceptions.h"
#include "../core/Logger.h"
#include "../core/X64DBGBridge.h"
#include "../utils/StringUtils.h"

namespace MCP {

void PatchHandler::RegisterMethods() {
    auto& d = MethodDispatcher::Instance();
    d.RegisterMethod("patch.list", PatchList);
    d.RegisterMethod("patch.restore", PatchRestore);
}

nlohmann::json PatchHandler::PatchList(const nlohmann::json&) {
    auto* funcs = DbgFunctions();
    if (!funcs || !funcs->PatchEnum)
        throw MCPException("Patch enumeration not available");

    size_t bufSize = 0;
    funcs->PatchEnum(nullptr, &bufSize);

    nlohmann::json patches = nlohmann::json::array();
    if (bufSize > 0) {
        size_t count = bufSize / sizeof(DBGPATCHINFO);
        std::vector<DBGPATCHINFO> patchBuf(count);
        if (funcs->PatchEnum(patchBuf.data(), &bufSize)) {
            for (size_t i = 0; i < count; ++i) {
                nlohmann::json entry;
                entry["address"] = StringUtils::FormatAddress(static_cast<uint64_t>(patchBuf[i].addr));
                entry["old_byte"] = patchBuf[i].oldbyte;
                entry["new_byte"] = patchBuf[i].newbyte;
                entry["module"] = std::string(patchBuf[i].mod);
                patches.push_back(entry);
            }
        }
    }

    nlohmann::json result;
    result["count"] = patches.size();
    result["patches"] = patches;
    return result;
}

nlohmann::json PatchHandler::PatchRestore(const nlohmann::json& params) {
    if (!params.contains("address"))
        throw InvalidParamsException("Missing required parameter: address");

    auto* funcs = DbgFunctions();
    if (!funcs || !funcs->PatchRestore)
        throw MCPException("Patch restore not available");

    uint64_t addr = StringUtils::ParseAddress(params["address"].get<std::string>());
    bool ok = funcs->PatchRestore(static_cast<duint>(addr));

    nlohmann::json result;
    result["address"] = StringUtils::FormatAddress(addr);
    result["success"] = ok;
    return result;
}

} // namespace MCP
