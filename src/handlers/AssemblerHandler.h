#pragma once
#include <nlohmann/json.hpp>

namespace MCP {

class AssemblerHandler {
public:
    static void RegisterMethods();
    static nlohmann::json Assemble(const nlohmann::json& params);
};

} // namespace MCP
