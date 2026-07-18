#include "core/ArchitectureRegisterNames.h"

#include <array>
#include <string_view>

namespace {

bool Contains(const std::array<std::string_view, 3>& fields, std::string_view expected) {
    for (const auto field : fields) {
        if (field == expected) {
            return true;
        }
    }
    return false;
}

} // namespace

int main() {
    const std::array<std::string_view, 3> responseFields = {
        MCP::ArchitectureRegisterNames::InstructionPointer,
        MCP::ArchitectureRegisterNames::StackPointer,
        MCP::ArchitectureRegisterNames::BasePointer
    };

#ifdef XDBG_ARCH_X64
    if (!Contains(responseFields, "rip") || !Contains(responseFields, "rsp") ||
        !Contains(responseFields, "rbp") || Contains(responseFields, "eip") ||
        Contains(responseFields, "esp") || Contains(responseFields, "ebp")) {
        return 1;
    }
#else
    if (!Contains(responseFields, "eip") || !Contains(responseFields, "esp") ||
        !Contains(responseFields, "ebp") || Contains(responseFields, "rip") ||
        Contains(responseFields, "rsp") || Contains(responseFields, "rbp")) {
        return 1;
    }
#endif
    return 0;
}
