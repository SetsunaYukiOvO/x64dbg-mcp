#pragma once
#include <nlohmann/json.hpp>

namespace MCP {

class PatchHandler {
public:
    static void RegisterMethods();
    static nlohmann::json PatchList(const nlohmann::json& params);
    static nlohmann::json PatchRestore(const nlohmann::json& params);
};

} // namespace MCP
