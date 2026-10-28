#include "core/UInt32Validation.h"
int main() {
    using nlohmann::json;
    for (const auto& value : {json(1), json("1"), json("0x1"), json(".1")})
        if (MCP::ValidateUInt32(value, "pid") != 1) return 1;
    if (MCP::ValidateUInt32(UINT32_MAX, "pid") != UINT32_MAX) return 2;
    for (const auto& value : {json(0), json(-1), json(4294967297ULL), json("4294967297"),
                            json("1junk"), json("-1"), json("+1"), json("0x"), json(1.5), json(true)}) {
        try { MCP::ValidateUInt32(value, "pid"); return 3; }
        catch (const MCP::InvalidParamsException&) {}
    }
    return 0;
}
