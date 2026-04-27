#include "BookmarkHandler.h"
#include "../core/MethodDispatcher.h"
#include "../core/Exceptions.h"
#include "../core/Logger.h"
#include "../core/X64DBGBridge.h"
#include "../utils/StringUtils.h"
#include <_scriptapi_bookmark.h>
#include <bridgelist.h>

namespace MCP {

void BookmarkHandler::RegisterMethods() {
    auto& d = MethodDispatcher::Instance();
    d.RegisterMethod("bookmark.set", Set);
    d.RegisterMethod("bookmark.delete", Delete);
    d.RegisterMethod("bookmark.list", List);
}

nlohmann::json BookmarkHandler::Set(const nlohmann::json& params) {
    if (!params.contains("address"))
        throw InvalidParamsException("Missing required parameter: address");

    uint64_t addr = StringUtils::ParseAddress(params["address"].get<std::string>());
    bool ok = Script::Bookmark::Set(static_cast<duint>(addr), true);

    nlohmann::json result;
    result["address"] = StringUtils::FormatAddress(addr);
    result["success"] = ok;
    return result;
}

nlohmann::json BookmarkHandler::Delete(const nlohmann::json& params) {
    if (!params.contains("address"))
        throw InvalidParamsException("Missing required parameter: address");

    uint64_t addr = StringUtils::ParseAddress(params["address"].get<std::string>());
    bool ok = Script::Bookmark::Delete(static_cast<duint>(addr));

    nlohmann::json result;
    result["address"] = StringUtils::FormatAddress(addr);
    result["success"] = ok;
    return result;
}

nlohmann::json BookmarkHandler::List(const nlohmann::json&) {
    BridgeList<Script::Bookmark::BookmarkInfo> list;
    Script::Bookmark::GetList(&list);

    nlohmann::json bookmarks = nlohmann::json::array();
    for (size_t i = 0; i < list.Count(); ++i) {
        const auto& b = list[i];
        nlohmann::json entry;
        entry["address"] = StringUtils::FormatAddress(static_cast<uint64_t>(b.rva));
        entry["module"] = std::string(b.mod);
        entry["manual"] = b.manual;
        bookmarks.push_back(entry);
    }

    nlohmann::json result;
    result["count"] = bookmarks.size();
    result["bookmarks"] = bookmarks;
    return result;
}

} // namespace MCP
