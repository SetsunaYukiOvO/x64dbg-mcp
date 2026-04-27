#pragma once
#include <nlohmann/json.hpp>

namespace MCP {

class ExpressionHandler {
public:
    static void RegisterMethods();
    static nlohmann::json Evaluate(const nlohmann::json& params);
};

} // namespace MCP
