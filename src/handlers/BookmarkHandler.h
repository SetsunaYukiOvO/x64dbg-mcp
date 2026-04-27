#pragma once
#include <nlohmann/json.hpp>

namespace MCP {

class BookmarkHandler {
public:
    static void RegisterMethods();
    static nlohmann::json Set(const nlohmann::json& params);
    static nlohmann::json Delete(const nlohmann::json& params);
    static nlohmann::json List(const nlohmann::json& params);
};

} // namespace MCP
