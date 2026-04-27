#pragma once
#include <nlohmann/json.hpp>

namespace MCP {

class AnalysisHandler {
public:
    static void RegisterMethods();
    static nlohmann::json XrefGet(const nlohmann::json& params);
    static nlohmann::json FunctionList(const nlohmann::json& params);
    static nlohmann::json FunctionGet(const nlohmann::json& params);
};

} // namespace MCP
