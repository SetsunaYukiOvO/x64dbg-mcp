#include "ExpressionHandler.h"
#include "../core/MethodDispatcher.h"
#include "../core/Exceptions.h"
#include "../core/Logger.h"
#include "../core/X64DBGBridge.h"
#include "../utils/StringUtils.h"

namespace MCP {

void ExpressionHandler::RegisterMethods() {
    auto& d = MethodDispatcher::Instance();
    d.RegisterMethod("eval.expression", Evaluate);
}

nlohmann::json ExpressionHandler::Evaluate(const nlohmann::json& params) {
    if (!params.contains("expression"))
        throw InvalidParamsException("Missing required parameter: expression");

    std::string expr = params["expression"].get<std::string>();
    bool success = false;
    duint value = DbgEval(expr.c_str(), &success);

    nlohmann::json result;
    result["expression"] = expr;
    result["valid"] = success;
    if (success) {
        result["result"] = StringUtils::FormatAddress(static_cast<uint64_t>(value));
        result["result_decimal"] = static_cast<uint64_t>(value);
    }
    return result;
}

} // namespace MCP
