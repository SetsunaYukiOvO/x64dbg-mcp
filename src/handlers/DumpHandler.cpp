#include "DumpHandler.h"
#include "../business/DumpManager.h"
#include "../core/MethodDispatcher.h"
#include "../core/PermissionChecker.h"
#include "../core/RequestValidator.h"
#include "../core/Exceptions.h"
#include "../core/Logger.h"
#include "../utils/StringUtils.h"
#include <_scriptapi_module.h>
#include <cctype>
#include <set>
#include <filesystem>

namespace MCP {
namespace {

bool ValidateOutputPath(const std::string& path, std::string& error) {
    if (path.empty()) {
        error = "output_path must not be empty";
        return false;
    }

    // Reject path traversal sequences
    if (path.find("..") != std::string::npos) {
        error = "output_path contains forbidden path traversal sequence (..)";
        return false;
    }

    // Reject raw UNC paths pointing to remote shares
    if (path.size() >= 2 && path[0] == '\\' && path[1] == '\\') {
        error = "output_path must not be a UNC network path";
        return false;
    }

    // Reject null-byte injection (bypass techniques)
    if (path.find('\0') != std::string::npos) {
        error = "output_path contains null bytes";
        return false;
    }

    // Validate path length
    if (path.size() > 32767) {
        error = "output_path exceeds maximum length";
        return false;
    }

    // Reject paths targeting sensitive system directories (case-insensitive)
    {
        std::string lower = path;
        for (auto& c : lower) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
        static const std::vector<std::string> sensitive = {
            "\\windows\\", "\\system32\\", "\\syswow64\\",
            "\\startup", "\\appdata\\roaming\\microsoft\\windows\\start menu",
            "\\systemroot\\"
        };
        for (const auto& dir : sensitive) {
            if (lower.find(dir) != std::string::npos) {
                error = "output_path targets a protected system directory";
                return false;
            }
        }
    }

    return true;
}

} // namespace

void DumpHandler::RegisterMethods() {
    auto& dispatcher = MethodDispatcher::Instance();

    dispatcher.RegisterMethod("dump.module", DumpModule);
    dispatcher.RegisterMethod("dump.memory_region", DumpMemoryRegion);
    dispatcher.RegisterMethod("dump.analyze_module", AnalyzeModule);
    dispatcher.RegisterMethod("dump.detect_oep", DetectOEP);
    dispatcher.RegisterMethod("dump.get_dumpable_regions", GetDumpableRegions);
}

nlohmann::json DumpHandler::DumpModule(const nlohmann::json& params) {
    // 妫€鏌ュ啓鏉冮檺
    if (!PermissionChecker::Instance().IsMemoryWriteAllowed()) {
        throw PermissionDeniedException("Dumping module requires write permission");
    }
    
    // 楠岃瘉鍙傛暟
    if (!params.contains("module")) {
        throw InvalidParamsException("Missing required parameter: module");
    }
    
    if (!params.contains("output_path")) {
        throw InvalidParamsException("Missing required parameter: output_path");
    }
    
    std::string module = params["module"].get<std::string>();
    std::string outputPath = params["output_path"].get<std::string>();

    {
        std::string pathError;
        if (!ValidateOutputPath(outputPath, pathError)) {
            throw InvalidParamsException(pathError);
        }
    }

    // Support both flattened tool arguments and nested `options` object.
    DumpOptions options;
    nlohmann::json nestedOptions = nlohmann::json::object();
    if (params.contains("options") && !params["options"].is_null()) {
        if (!params["options"].is_object()) {
            throw InvalidParamsException("Parameter 'options' must be an object");
        }
        nestedOptions = params["options"];
    }

    auto readBoolOption = [&](const char* key, bool defaultValue) {
        if (nestedOptions.contains(key)) {
            return nestedOptions[key].get<bool>();
        }
        if (params.contains(key)) {
            return params[key].get<bool>();
        }
        return defaultValue;
    };

    options.fixOEP = readBoolOption("fix_oep", true);
    options.removeIntegrityCheck = readBoolOption("remove_integrity_check", true);
    options.rebuildPE = readBoolOption("rebuild_pe", true);
    options.autoDetectOEP = readBoolOption("auto_detect_oep", false);
    options.dumpFullImage = readBoolOption("dump_full_image", false);

    const bool nestedHasOEP = nestedOptions.contains("oep");
    const bool topLevelHasOEP = params.contains("oep");
    if (nestedHasOEP || topLevelHasOEP) {
        const nlohmann::json& oepNode = nestedHasOEP ? nestedOptions["oep"] : params["oep"];
        if (!oepNode.is_string() && !oepNode.is_number_integer()) {
            throw InvalidParamsException("Parameter 'oep' must be a string or integer");
        }

        const uint64_t forcedOEP = RequestValidator::ValidateAddress(oepNode);
        options.forcedOEP = forcedOEP;
        options.fixOEP = true;
    }
    
    auto& manager = DumpManager::Instance();
    
    // 鎵цdump
    auto result = manager.DumpModule(module, outputPath, options, nullptr);
    
    return DumpResultToJson(result);
}

nlohmann::json DumpHandler::DumpMemoryRegion(const nlohmann::json& params) {
    // 妫€鏌ュ啓鏉冮檺
    if (!PermissionChecker::Instance().IsMemoryWriteAllowed()) {
        throw PermissionDeniedException("Dumping memory requires write permission");
    }
    
    // 楠岃瘉鍙傛暟
    if (!params.contains("address")) {
        throw InvalidParamsException("Missing required parameter: address");
    }
    
    if (!params.contains("size")) {
        throw InvalidParamsException("Missing required parameter: size");
    }
    
    if (!params.contains("output_path")) {
        throw InvalidParamsException("Missing required parameter: output_path");
    }
    
    uint64_t address = RequestValidator::GetAddress(params, "address");
    size_t size = params["size"].get<size_t>();
    std::string outputPath = params["output_path"].get<std::string>();

    {
        std::string pathError;
        if (!ValidateOutputPath(outputPath, pathError)) {
            throw InvalidParamsException(pathError);
        }
    }

    bool asRawBinary = params.value("as_raw_binary", false);
    
    auto& manager = DumpManager::Instance();
    auto result = manager.DumpMemoryRegion(address, size, outputPath, asRawBinary);
    
    return DumpResultToJson(result);
}

nlohmann::json DumpHandler::AnalyzeModule(const nlohmann::json& params) {
    std::string module;
    
    // 濡傛灉娌℃湁鎻愪緵 module 鍙傛暟,浣跨敤涓绘ā鍧?
    if (!params.contains("module") || params["module"].is_null()) {
        Script::Module::ModuleInfo info;
        if (Script::Module::GetMainModuleInfo(&info)) {
            // Use base address to avoid any encoding ambiguity in Script API module names.
            module = StringUtils::FormatAddress(static_cast<uint64_t>(info.base));
            Logger::Info("No module specified, using main module at: {}", module);
        } else {
            throw MCPException("No module specified and failed to get main module", -32000);
        }
    } else {
        module = params["module"].get<std::string>();
    }
    
    auto& manager = DumpManager::Instance();
    auto info = manager.AnalyzeModule(module);
    
    return ModuleDumpInfoToJson(info);
}

nlohmann::json DumpHandler::DetectOEP(const nlohmann::json& params) {
    // 楠岃瘉鍙傛暟
    if (!params.contains("module")) {
        throw InvalidParamsException("Missing required parameter: module");
    }
    
    std::string moduleStr = params["module"].get<std::string>();
    auto& manager = DumpManager::Instance();
    
    // 瑙ｆ瀽妯″潡鍚嶆垨鍦板潃
    auto moduleBaseOpt = manager.ParseModuleOrAddress(moduleStr);
    if (!moduleBaseOpt.has_value()) {
        throw InvalidParamsException("Invalid module name or address: " + moduleStr);
    }
    
    uint64_t moduleBase = moduleBaseOpt.value();
    auto oepOpt = manager.DetectOEP(moduleBase);

    nlohmann::json result;
    result["detected"] = oepOpt.has_value();
    
    if (oepOpt.has_value()) {
        uint64_t oep = oepOpt.value();
        result["oep"] = StringUtils::FormatAddress(oep);
        result["rva"] = StringUtils::FormatAddress(oep - moduleBase);
    } else {
        result["oep"] = nullptr;
        result["rva"] = nullptr;
    }
    
    return result;
}

nlohmann::json DumpHandler::GetDumpableRegions(const nlohmann::json& params) {
    uint64_t moduleBase = 0;
    
    if (params.contains("module_base")) {
        moduleBase = RequestValidator::GetAddress(params, "module_base");
    }
    
    auto& manager = DumpManager::Instance();
    auto regions = manager.GetDumpableRegions(moduleBase);
    
    nlohmann::json regionArray = nlohmann::json::array();
    for (const auto& region : regions) {
        regionArray.push_back(MemoryRegionDumpToJson(region));
    }
    
    nlohmann::json result;
    result["regions"] = regionArray;
    result["count"] = regions.size();
    
    return result;
}
nlohmann::json DumpHandler::DumpResultToJson(const DumpResult& result) {
    nlohmann::json json;
    
    json["success"] = result.success;
    
    if (result.success) {
        json["file_path"] = result.filePath;
        json["dumped_size"] = result.dumpedSize;
        
        if (result.originalEP != 0) {
            json["original_ep"] = StringUtils::FormatAddress(result.originalEP);
        }
        
        if (result.newEP != 0) {
            json["new_ep"] = StringUtils::FormatAddress(result.newEP);
        }
        
        // 杩涘害淇℃伅
        json["stage"] = static_cast<int>(result.finalProgress.stage);
        json["progress"] = result.finalProgress.progress;
        json["message"] = result.finalProgress.message;
    } else {
        json["error"] = result.error;
        json["stage"] = static_cast<int>(result.finalProgress.stage);
        json["message"] = result.finalProgress.message;
    }
    
    return json;
}

nlohmann::json DumpHandler::ModuleDumpInfoToJson(const ModuleDumpInfo& info) {
    nlohmann::json json;
    
    json["name"] = StringUtils::FixUtf8Mojibake(info.name);
    json["path"] = StringUtils::FixUtf8Mojibake(info.path);
    json["base_address"] = StringUtils::FormatAddress(info.baseAddress);
    json["size"] = info.size;
    json["entry_point"] = StringUtils::FormatAddress(info.entryPoint);
    json["is_packed"] = info.isPacked;
    json["packer_id"] = info.packerId;
    
    return json;
}

nlohmann::json DumpHandler::MemoryRegionDumpToJson(const MemoryRegionDump& region) {
    nlohmann::json json;
    
    json["address"] = StringUtils::FormatAddress(region.address);
    json["size"] = region.size;
    json["protection"] = region.protection;
    json["type"] = region.type;
    json["name"] = region.name;
    
    return json;
}

} // namespace MCP

