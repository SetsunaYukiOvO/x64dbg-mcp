#include "communication/HttpHeaders.h"
int main() {
    MCP::HttpHeaders h;
    if (!MCP::ParseHttpHeaders("POST /rpc HTTP/1.1\r\nX-Origin: wrong\r\nOrigin: https://good\r\n\r\n", h) ||
        h.at("origin") != "https://good") return 1;
    if (!MCP::ParseHttpHeaders("POST /rpc HTTP/1.1\r\nAuthorization: Bearer token\r\n\r\n", h) ||
        h.at("authorization") != "Bearer token") return 2;
    for (const auto& fields : {"Origin: a\r\nORIGIN: b", "Origin : a", " bad: a", "Bad", "Transfer-Encoding: chunked"}) {
        if (MCP::ParseHttpHeaders(std::string("POST /rpc HTTP/1.1\r\n") + fields + "\r\n\r\n", h)) return 3;
    }
    for (const auto& length : {"-1", "+1", "1z", "184467440737095516160", ""}) {
        h = {{"content-length", length}};
        size_t n;
        if (MCP::HttpContentLength(h, n)) return 4;
    }
    return 0;
}
