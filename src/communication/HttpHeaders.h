#pragma once
#include <algorithm>
#include <cctype>
#include <limits>
#include <map>
#include <string>

namespace MCP {
using HttpHeaders = std::map<std::string, std::string>;

inline bool ParseHttpHeaders(const std::string& request, HttpHeaders& headers) {
    headers.clear();
    const size_t end = request.find("\r\n\r\n");
    size_t start = request.find("\r\n");
    if (end == std::string::npos || start == std::string::npos) return false;
    start += 2;
    while (start < end) {
        const size_t lineEnd = request.find("\r\n", start);
        const size_t colon = request.find(':', start);
        if (lineEnd == std::string::npos || colon == std::string::npos ||
            colon >= lineEnd || colon == start) return false;
        std::string name = request.substr(start, colon - start);
        for (unsigned char c : name) {
            if (!std::isalnum(c) && std::string("!#$%&'*+-.^_`|~").find(c) == std::string::npos)
                return false;
        }
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        std::string value = request.substr(colon + 1, lineEnd - colon - 1);
        for (unsigned char c : value) {
            if ((c < 32 && c != '\t') || c == 127) return false;
        }
        const auto first = value.find_first_not_of(" \t");
        value = first == std::string::npos ? "" : value.substr(first, value.find_last_not_of(" \t") - first + 1);
        // Reject ambiguous duplicate fields, including authentication and framing.
        if (!headers.emplace(name, value).second) return false;
        start = lineEnd + 2;
    }
    return headers.count("transfer-encoding") == 0;
}

inline bool HttpContentLength(const HttpHeaders& headers, size_t& length) {
    length = 0;
    const auto it = headers.find("content-length");
    if (it == headers.end()) return true;
    if (it->second.empty()) return false;
    for (unsigned char c : it->second) {
        if (c < '0' || c > '9' || length > (std::numeric_limits<size_t>::max() - (c - '0')) / 10)
            return false;
        length = length * 10 + (c - '0');
    }
    return true;
}
} // namespace MCP
