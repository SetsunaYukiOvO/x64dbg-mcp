#pragma once
#include "Exceptions.h"
#include <cstdint>
#include <string>
#include <nlohmann/json.hpp>

namespace MCP {
inline uint32_t ValidateUInt32(const nlohmann::json& value, const std::string& name, uint32_t minimum = 1) {
    uint64_t parsed = 0;
    bool valid = true;
    if (value.is_number_unsigned()) parsed = value.get<uint64_t>();
    else if (value.is_number_integer()) {
        const auto number = value.get<int64_t>();
        valid = number >= 0;
        parsed = static_cast<uint64_t>(number);
    } else if (value.is_string()) {
        std::string text = value.get<std::string>();
        const auto begin = text.find_first_not_of(" \t");
        text = begin == std::string::npos ? "" : text.substr(begin, text.find_last_not_of(" \t") - begin + 1);
        unsigned base = 10;
        size_t i = 0;
        if (text.rfind("0x", 0) == 0 || text.rfind("0X", 0) == 0) { base = 16; i = 2; }
        else if (!text.empty() && text[0] == '.') i = 1;
        valid = i < text.size();
        for (; valid && i < text.size(); ++i) {
            const char c = text[i];
            const unsigned digit = c >= '0' && c <= '9' ? c - '0' :
                c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : 255;
            if (digit >= base || parsed > (UINT32_MAX - digit) / base) { valid = false; break; }
            parsed = parsed * base + digit;
        }
    } else valid = false;
    if (!valid || parsed < minimum || parsed > UINT32_MAX)
        throw InvalidParamsException(name + " must be an integer in " + std::to_string(minimum) + "..4294967295");
    return static_cast<uint32_t>(parsed);
}
} // namespace MCP
