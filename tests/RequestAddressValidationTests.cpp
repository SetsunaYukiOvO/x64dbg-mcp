#include "core/Exceptions.h"
#include "core/RequestValidator.h"
#include "utils/StringUtils.h"

#include <cstdint>
#include <optional>
#include <string>

namespace {

std::optional<uint64_t> ResolveBoundaryExpression(const std::string& expression) {
    if (expression == "max_target") {
        return UINT64_C(0xFFFFFFFF);
    }
    if (expression == "over_target") {
        return UINT64_C(0x100000000);
    }
    return std::nullopt;
}

bool RejectsAddress(const nlohmann::json& value) {
    try {
        (void)MCP::RequestValidator::ValidateAddress(value);
        return false;
    } catch (const MCP::InvalidAddressException&) {
        return true;
    }
}

std::optional<uint64_t> RejectMalformedNumericExpression(const std::string&) {
    return std::nullopt;
}

} // namespace

int main() {
    MCP::StringUtils::SetAddressResolver(RejectMalformedNumericExpression);
    if (!RejectsAddress(nlohmann::json("0x1000junk")) ||
        !RejectsAddress(nlohmann::json("-1"))) {
        return 1;
    }

#ifndef XDBG_ARCH_X86
    MCP::StringUtils::SetAddressResolver(nullptr);
    return 0;
#else
    MCP::StringUtils::SetAddressResolver(ResolveBoundaryExpression);

    if (MCP::RequestValidator::ValidateAddress(nlohmann::json(UINT64_C(0xFFFFFFFF))) !=
            UINT64_C(0xFFFFFFFF) ||
        !RejectsAddress(nlohmann::json(UINT64_C(0x100000000))) ||
        MCP::RequestValidator::ValidateAddress(std::string("0xFFFFFFFF")) !=
            UINT64_C(0xFFFFFFFF) ||
        !RejectsAddress(nlohmann::json("0x100000000")) ||
        MCP::RequestValidator::ValidateAddress(std::string("max_target")) !=
            UINT64_C(0xFFFFFFFF) ||
        !RejectsAddress(nlohmann::json("over_target")) ||
        !RejectsAddress(nlohmann::json(-1))) {
        return 2;
    }

    if (MCP::RequestValidator::GetAddress(
            nlohmann::json{{"address", UINT64_C(0xFFFFFFFF)}}, "address") !=
            UINT64_C(0xFFFFFFFF) ||
        !RejectsAddress(nlohmann::json(UINT64_C(0x100000000))) ||
        MCP::RequestValidator::TryValidateAddressOrName("kernel32.dll").has_value() ||
        MCP::RequestValidator::TryValidateAddressOrName("max_target").value_or(0) !=
            UINT64_C(0xFFFFFFFF)) {
        return 3;
    }

    bool mixedAddressRejected = false;
    try {
        (void)MCP::RequestValidator::TryValidateAddressOrName("over_target");
    } catch (const MCP::InvalidAddressException&) {
        mixedAddressRejected = true;
    }
    if (!mixedAddressRejected) {
        return 4;
    }

    MCP::StringUtils::SetAddressResolver(nullptr);
    return 0;
#endif
}
