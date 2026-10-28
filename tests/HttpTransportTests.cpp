#include "communication/MCPHttpServer.h"
#include "core/ConfigManager.h"
#include "core/MethodDispatcher.h"
#include <ws2tcpip.h>
#include <stdexcept>
#include <iostream>

namespace MCP {
MethodDispatcher& MethodDispatcher::Instance() { static MethodDispatcher instance; return instance; }
JSONRPCResponse MethodDispatcher::Dispatch(const JSONRPCRequest& request) {
    return {"2.0", request.id, {{"method", request.method}}, std::nullopt};
}
}

namespace {
int port;
SOCKET Connect() {
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<u_short>(port));
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    if (connect(s, reinterpret_cast<sockaddr*>(&address), sizeof(address))) throw std::runtime_error("connect");
    DWORD timeout = 1500;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&timeout), sizeof(timeout));
    return s;
}
std::string Read(SOCKET s) {
    char buffer[8192];
    int n = recv(s, buffer, sizeof(buffer), 0);
    return n > 0 ? std::string(buffer, n) : "";
}
std::string OpenStream(SOCKET s) {
    const std::string request = "GET /sse HTTP/1.1\r\nHost: localhost\r\n\r\n";
    send(s, request.data(), static_cast<int>(request.size()), 0);
    std::string data;
    while (data.find("/message?sessionId=") == std::string::npos || data.rfind("\r\n\r\n") < data.find("/message?sessionId=")) {
        const auto part = Read(s);
        if (part.empty()) throw std::runtime_error("SSE handshake");
        data += part;
    }
    const auto start = data.find("/message?sessionId=");
    return data.substr(start, data.find("\r\n", start) - start);
}
std::string Post(const std::string& path) {
    SOCKET s = Connect();
    const std::string body = R"({"jsonrpc":"2.0","id":73,"method":"initialize"})";
    const std::string request = "POST " + path + " HTTP/1.1\r\nHost: localhost\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    send(s, request.data(), static_cast<int>(request.size()), 0);
    std::string response, part;
    while (!(part = Read(s)).empty()) response += part;
    closesocket(s);
    return response;
}
}

int main() {
    MCP::MCPHttpServer server;
    for (port = 18300; port < 18400; ++port) if (server.Start("127.0.0.1", port)) break;
    if (!server.IsRunning()) return 1;
    try {
        SOCKET first = Connect(), second = Connect();
        const auto endpoint = OpenStream(first), other = OpenStream(second);
        const auto reply = Post(endpoint);
        if (endpoint == other || reply.find("202") == std::string::npos) { std::cerr << endpoint << "\n" << other << "\n" << reply; return 2; }
        if (Read(first).find("\"id\":73") == std::string::npos || !Read(second).empty()) return 3;
        if (Post("/rpc").find("\"id\":73") == std::string::npos) return 4;
        if (Post("/message?sessionId=unknown").find("404") == std::string::npos) return 5;
        closesocket(first); closesocket(second);
        server.Stop();
    } catch (const std::exception& ex) { std::cerr << ex.what(); return 10; }
    return 0;
}
