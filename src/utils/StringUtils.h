#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <optional>
#ifdef _WIN32
#include <windows.h>
#endif

namespace MCP {
namespace StringUtils {

inline bool IsValidUtf8(const std::string& input);

/**
 * @brief 转换字符串为小写
 */
inline std::string ToLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

/**
 * @brief 转换字符串为大写
 */
inline std::string ToUpper(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return result;
}

/**
 * @brief 去除字符串首尾空白
 */
inline std::string Trim(const std::string& str) {
    auto start = std::find_if_not(str.begin(), str.end(),
                                   [](unsigned char c) { return std::isspace(c); });
    auto end = std::find_if_not(str.rbegin(), str.rend(),
                                 [](unsigned char c) { return std::isspace(c); }).base();
    return (start < end) ? std::string(start, end) : std::string();
}

/**
 * @brief 分割字符串
 */
inline std::vector<std::string> Split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

/**
 * @brief 连接字符串向量
 */
inline std::string Join(const std::vector<std::string>& parts, const std::string& delimiter) {
    if (parts.empty()) return "";
    
    std::ostringstream oss;
    oss << parts[0];
    for (size_t i = 1; i < parts.size(); ++i) {
        oss << delimiter << parts[i];
    }
    return oss.str();
}

/**
 * @brief 检查字符串是否以指定前缀开始
 */
inline bool StartsWith(const std::string& str, const std::string& prefix) {
    return str.size() >= prefix.size() &&
           str.compare(0, prefix.size(), prefix) == 0;
}

/**
 * @brief 检查字符串是否以指定后缀结束
 */
inline bool EndsWith(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

/**
 * @brief 替换字符串中的所有匹配项
 */
inline std::string ReplaceAll(std::string str, const std::string& from, const std::string& to) {
    if (from.empty()) return str;
    
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    return str;
}

/**
 * @brief 十六进制字符串转字节数组
 */
inline std::vector<uint8_t> HexToBytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    std::string cleanHex = hex;
    
    // 移除 0x 前缀
    if (StartsWith(cleanHex, "0x") || StartsWith(cleanHex, "0X")) {
        cleanHex = cleanHex.substr(2);
    }
    
    // 移除空格
    cleanHex.erase(std::remove_if(cleanHex.begin(), cleanHex.end(), ::isspace), cleanHex.end());
    
    // 确保偶数长度
    if (cleanHex.length() % 2 != 0) {
        cleanHex = "0" + cleanHex;
    }
    
    for (size_t i = 0; i < cleanHex.length(); i += 2) {
        std::string byteStr = cleanHex.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16));
        bytes.push_back(byte);
    }
    
    return bytes;
}

/**
 * @brief 字节数组转十六进制字符串
 */
inline std::string BytesToHex(const uint8_t* data, size_t size, bool uppercase = true) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    if (uppercase) {
        oss << std::uppercase;
    }
    
    for (size_t i = 0; i < size; ++i) {
        oss << std::setw(2) << static_cast<int>(data[i]);
    }
    
    return oss.str();
}

/**
 * @brief 字节数组转十六进制字符串（vector 版本）
 */
inline std::string BytesToHex(const std::vector<uint8_t>& bytes, bool uppercase = true) {
    return BytesToHex(bytes.data(), bytes.size(), uppercase);
}

/**
 * @brief 格式化地址为十六进制字符串
 */
inline std::string FormatAddress(uint64_t address, bool prefix = true) {
    std::ostringstream oss;
    if (prefix) {
        oss << "0x";
    }
    oss << std::hex << std::uppercase << std::setfill('0') << std::setw(16) << address;
    return oss.str();
}

/**
 * @brief Try resolving an address string via x64dbg's expression evaluator (DbgEval).
 * Handles symbol names, register names, and expressions.
 * Returns std::nullopt if DbgEval is unavailable or the expression fails.
 */
/**
 * @brief Callback type for resolving expressions (symbol names, register names, etc.)
 * to addresses. Set by the plugin at init time to wire in DbgEval.
 */
using AddressResolverFn = std::optional<uint64_t>(*)(const std::string&);
inline AddressResolverFn g_addressResolver = nullptr;

inline void SetAddressResolver(AddressResolverFn fn) { g_addressResolver = fn; }

/**
 * @brief Parse address string. Supports hex (0x...), decimal, and — when a
 * resolver is registered — symbol names, register names, and x64dbg expressions.
 */
inline uint64_t ParseAddress(const std::string& str) {
    std::string cleanStr = Trim(str);

    if (cleanStr.empty()) {
        throw std::invalid_argument("Empty address string");
    }

    // Fast path: try numeric parsing first
    try {
        int base = 10;
        std::string numStr = cleanStr;
        if (StartsWith(numStr, "0x") || StartsWith(numStr, "0X")) {
            base = 16;
            numStr = numStr.substr(2);
        } else if (std::all_of(numStr.begin(), numStr.end(),
                               [](char c) { return std::isxdigit(c); })) {
            base = 16;
        }
        return std::stoull(numStr, nullptr, base);
    } catch (...) {
        // Not a plain number — fall through to resolver
    }

    // Fallback: resolve via registered callback (DbgEval when running inside x64dbg)
    if (g_addressResolver) {
        auto resolved = g_addressResolver(cleanStr);
        if (resolved.has_value()) {
            return resolved.value();
        }
    }

    throw std::invalid_argument("Cannot resolve address: " + cleanStr);
}

/**
 * @brief 格式化字节大小（KB, MB, GB）
 */
inline std::string FormatSize(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unitIndex < 4) {
        size /= 1024.0;
        ++unitIndex;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << units[unitIndex];
    return oss.str();
}

/**
 * @brief 通配符模式匹配（支持 * 和 ?）
 */
inline bool WildcardMatch(const std::string& pattern, const std::string& str) {
    size_t pIdx = 0, sIdx = 0;
    size_t starIdx = std::string::npos, matchIdx = 0;
    
    while (sIdx < str.length()) {
        if (pIdx < pattern.length() && (pattern[pIdx] == str[sIdx] || pattern[pIdx] == '?')) {
            ++pIdx;
            ++sIdx;
        } else if (pIdx < pattern.length() && pattern[pIdx] == '*') {
            starIdx = pIdx;
            matchIdx = sIdx;
            ++pIdx;
        } else if (starIdx != std::string::npos) {
            pIdx = starIdx + 1;
            ++matchIdx;
            sIdx = matchIdx;
        } else {
            return false;
        }
    }
    
    while (pIdx < pattern.length() && pattern[pIdx] == '*') {
        ++pIdx;
    }
    
    return pIdx == pattern.length();
}

/**
 * @brief UTF-8 感知的通配符匹配（? 按 Unicode 码点匹配）
 */
inline bool WildcardMatchUtf8(const std::string& pattern, const std::string& str) {
#ifdef _WIN32
    if (!IsValidUtf8(pattern) || !IsValidUtf8(str)) {
        return WildcardMatch(pattern, str);
    }

    const int patternWideLen = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, pattern.data(), static_cast<int>(pattern.size()), nullptr, 0
    );
    const int strWideLen = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, str.data(), static_cast<int>(str.size()), nullptr, 0
    );
    if (patternWideLen <= 0 || strWideLen <= 0) {
        return WildcardMatch(pattern, str);
    }

    std::wstring patternWide(static_cast<size_t>(patternWideLen), L'\0');
    std::wstring strWide(static_cast<size_t>(strWideLen), L'\0');
    MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, pattern.data(), static_cast<int>(pattern.size()),
        patternWide.data(), patternWideLen
    );
    MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, str.data(), static_cast<int>(str.size()),
        strWide.data(), strWideLen
    );

    size_t pIdx = 0, sIdx = 0;
    size_t starIdx = std::wstring::npos, matchIdx = 0;

    while (sIdx < strWide.length()) {
        if (pIdx < patternWide.length() &&
            (patternWide[pIdx] == strWide[sIdx] || patternWide[pIdx] == L'?')) {
            ++pIdx;
            ++sIdx;
        } else if (pIdx < patternWide.length() && patternWide[pIdx] == L'*') {
            starIdx = pIdx;
            matchIdx = sIdx;
            ++pIdx;
        } else if (starIdx != std::wstring::npos) {
            pIdx = starIdx + 1;
            ++matchIdx;
            sIdx = matchIdx;
        } else {
            return false;
        }
    }

    while (pIdx < patternWide.length() && patternWide[pIdx] == L'*') {
        ++pIdx;
    }

    return pIdx == patternWide.length();
#else
    return WildcardMatch(pattern, str);
#endif
}

/**
 * @brief 将字节数组编码为 Base64 字符串
 */
inline std::string ToBase64(const std::vector<uint8_t>& data) {
    static const char base64_chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    
    std::string result;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    size_t in_len = data.size();
    const unsigned char* bytes_to_encode = data.data();
    
    while (in_len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            
            for (i = 0; i < 4; i++)
                result += base64_chars[char_array_4[i]];
            i = 0;
        }
    }
    
    if (i) {
        for (j = i; j < 3; j++)
            char_array_3[j] = '\0';
        
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        
        for (j = 0; j < i + 1; j++)
            result += base64_chars[char_array_4[j]];
        
        while (i++ < 3)
            result += '=';
    }
    
    return result;
}

/**
 * @brief 将 Base64 字符串解码为字节数组
 */
inline std::vector<uint8_t> FromBase64(const std::string& encoded_string) {
    static const std::string base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    
    auto is_base64 = [](unsigned char c) {
        return (isalnum(c) || (c == '+') || (c == '/'));
    };
    
    size_t in_len = encoded_string.size();
    int i = 0;
    int j = 0;
    int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::vector<uint8_t> result;
    
    while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
        char_array_4[i++] = encoded_string[in_]; in_++;
        if (i == 4) {
            for (i = 0; i < 4; i++)
                char_array_4[i] = static_cast<unsigned char>(base64_chars.find(char_array_4[i]));
            
            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
            
            for (i = 0; i < 3; i++)
                result.push_back(char_array_3[i]);
            i = 0;
        }
    }
    
    if (i) {
        for (j = i; j < 4; j++)
            char_array_4[j] = 0;
        
        for (j = 0; j < 4; j++)
            char_array_4[j] = static_cast<unsigned char>(base64_chars.find(char_array_4[j]));
        
        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        
        for (j = 0; j < i - 1; j++)
            result.push_back(char_array_3[j]);
    }
    
    return result;
}

/**
 * @brief 检查字符串是否是有效 UTF-8
 */
inline bool IsValidUtf8(const std::string& input) {
#ifdef _WIN32
    if (input.empty()) {
        return true;
    }

    const int wideLen = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        input.data(),
        static_cast<int>(input.size()),
        nullptr,
        0
    );
    return wideLen > 0;
#else
    (void)input;
    return true;
#endif
}

/**
 * @brief 修复 UTF-8 二次编码导致的 mojibake（如 "åçº§" -> "初级"）
 */
inline std::string FixUtf8Mojibake(const std::string& input) {
#ifdef _WIN32
    if (input.empty() || !IsValidUtf8(input)) {
        return input;
    }

    const int wideLen = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        input.data(),
        static_cast<int>(input.size()),
        nullptr,
        0
    );
    if (wideLen <= 0) {
        return input;
    }

    std::wstring wide(static_cast<size_t>(wideLen), L'\0');
    MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        input.data(),
        static_cast<int>(input.size()),
        wide.data(),
        wideLen
    );

    bool hasHighByteChars = false;
    for (wchar_t ch : wide) {
        if (ch > 0x00FF) {
            // Already a normal Unicode string, not mojibake.
            return input;
        }
        if (ch >= 0x0080) {
            hasHighByteChars = true;
        }
    }

    if (!hasHighByteChars) {
        return input;
    }

    std::string recoveredBytes;
    recoveredBytes.reserve(wide.size());
    for (wchar_t ch : wide) {
        recoveredBytes.push_back(static_cast<char>(static_cast<unsigned char>(ch)));
    }

    if (!IsValidUtf8(recoveredBytes)) {
        return input;
    }

    // Ensure the recovered bytes decode to non-ASCII text to avoid over-correction.
    const int recoveredWideLen = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        recoveredBytes.data(),
        static_cast<int>(recoveredBytes.size()),
        nullptr,
        0
    );
    if (recoveredWideLen <= 0) {
        return input;
    }

    std::wstring recoveredWide(static_cast<size_t>(recoveredWideLen), L'\0');
    MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        recoveredBytes.data(),
        static_cast<int>(recoveredBytes.size()),
        recoveredWide.data(),
        recoveredWideLen
    );

    const bool hasNonAscii = std::any_of(
        recoveredWide.begin(),
        recoveredWide.end(),
        [](wchar_t ch) { return ch > 0x007F; }
    );
    if (!hasNonAscii) {
        return input;
    }

    return recoveredBytes;
#else
    return input;
#endif
}

} // namespace StringUtils
} // namespace MCP
