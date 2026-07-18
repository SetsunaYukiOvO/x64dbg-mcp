#include "business/RegisterManager.h"

#include <cstdint>

int main() {
    if (!MCP::RegisterManager::FitsRegisterValue(UINT64_C(0xFFFFFFFF), 4) ||
        MCP::RegisterManager::FitsRegisterValue(UINT64_C(0x100000000), 4)) {
        return 1;
    }

    return 0;
}
