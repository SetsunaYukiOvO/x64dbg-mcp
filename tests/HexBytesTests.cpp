#include "utils/StringUtils.h"
int main() {
    if (MCP::StringUtils::HexToBytes("0x00 aF 10") != std::vector<uint8_t>({0, 0xaf, 0x10})) return 1;
    for (const auto* text : {"1Z", "Z1", "-1", "+1", "1", "0xF", "00gg", "0x0x12"}) {
        try { MCP::StringUtils::HexToBytes(text); return 2; }
        catch (const std::invalid_argument&) {}
    }
    return 0;
}
