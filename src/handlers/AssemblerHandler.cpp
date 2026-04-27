#include "AssemblerHandler.h"
#include "../core/MethodDispatcher.h"
#include "../core/Exceptions.h"
#include "../core/Logger.h"
#include "../core/X64DBGBridge.h"
#include "../utils/StringUtils.h"
#include "../core/PermissionChecker.h"
#include <_scriptapi_assembler.h>

namespace MCP {

void AssemblerHandler::RegisterMethods() {
    auto& d = MethodDispatcher::Instance();
    d.RegisterMethod("assembler.assemble", Assemble);
}

nlohmann::json AssemblerHandler::Assemble(const nlohmann::json& params) {
    if (!params.contains("instruction"))
        throw InvalidParamsException("Missing required parameter: instruction");
    if (!params.contains("address"))
        throw InvalidParamsException("Missing required parameter: address");

    std::string instruction = params["instruction"].get<std::string>();
    uint64_t addr = StringUtils::ParseAddress(params["address"].get<std::string>());
    bool writeToMemory = params.value("write_to_memory", false);

    if (writeToMemory) {
        if (!PermissionChecker::Instance().IsMemoryWriteAllowed())
            throw PermissionDeniedException("Writing assembled bytes to memory requires write permission");

        int size = 0;
        char error[256] = {};
        bool ok = Script::Assembler::AssembleMemEx(
            static_cast<duint>(addr), instruction.c_str(), &size, error, true);

        if (!ok)
            throw MCPException(std::string("Assembly failed: ") + error);

        nlohmann::json result;
        result["address"] = StringUtils::FormatAddress(addr);
        result["size"] = size;
        result["written"] = true;
        result["instruction"] = instruction;
        return result;
    }

    unsigned char dest[16] = {};
    int size = 0;
    char error[256] = {};
    bool ok = Script::Assembler::AssembleEx(
        static_cast<duint>(addr), dest, &size, instruction.c_str(), error);

    if (!ok)
        throw MCPException(std::string("Assembly failed: ") + error);

    std::string hex;
    for (int i = 0; i < size; ++i) {
        char buf[4];
        snprintf(buf, sizeof(buf), "%02X", dest[i]);
        hex += buf;
    }

    nlohmann::json result;
    result["address"] = StringUtils::FormatAddress(addr);
    result["bytes"] = hex;
    result["size"] = size;
    result["written"] = false;
    result["instruction"] = instruction;
    return result;
}

} // namespace MCP
