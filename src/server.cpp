#include "server.hpp"
#include "pipeline.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_type = SOCKET;
static constexpr socket_type INVALID_SOCKET_VALUE = INVALID_SOCKET;
static void closeSocket(socket_type socket) { ::closesocket(socket); }
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_type = int;
static constexpr socket_type INVALID_SOCKET_VALUE = -1;
static void closeSocket(socket_type socket) { ::close(socket); }
#endif

#ifndef MINICPP_WEB_DIR
#define MINICPP_WEB_DIR "web"
#endif

namespace {
std::string readFile(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return {};
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}
void sendAll(socket_type socket, const std::string& data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
#ifdef _WIN32
        int count = ::send(socket, data.data() + sent, static_cast<int>(std::min<std::size_t>(data.size() - sent, 1024 * 1024)), 0);
#else
        ssize_t count = ::send(socket, data.data() + sent, data.size() - sent, 0);
#endif
        if (count <= 0) return;
        sent += static_cast<std::size_t>(count);
    }
}
void respond(socket_type client, int status, const std::string& contentType, const std::string& body) {
    std::string label = status == 200 ? "OK" : status == 404 ? "Not Found" : "Bad Request";
    std::ostringstream header;
    header << "HTTP/1.1 " << status << ' ' << label << "\r\n"
           << "Content-Type: " << contentType << "; charset=utf-8\r\n"
           << "Content-Length: " << body.size() << "\r\n"
           << "Cache-Control: no-store\r\nConnection: close\r\n\r\n";
    sendAll(client, header.str() + body);
}
void handleClient(socket_type client) {
    std::string request;
    char buffer[8192];
    std::size_t headerEnd = std::string::npos;
    std::size_t contentLength = 0;
    while (request.size() < 12 * 1024 * 1024) {
#ifdef _WIN32
        int count = ::recv(client, buffer, static_cast<int>(sizeof(buffer)), 0);
#else
        ssize_t count = ::recv(client, buffer, sizeof(buffer), 0);
#endif
        if (count <= 0) break;
        request.append(buffer, static_cast<std::size_t>(count));
        if (headerEnd == std::string::npos) {
            headerEnd = request.find("\r\n\r\n");
            if (headerEnd != std::string::npos) {
                std::string headers = request.substr(0, headerEnd);
                std::istringstream lines(headers);
                std::string line;
                std::getline(lines, line);
                while (std::getline(lines, line)) {
                    std::transform(line.begin(), line.end(), line.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                    auto pos = line.find("content-length:");
                    if (pos != std::string::npos) contentLength = static_cast<std::size_t>(std::stoul(line.substr(pos + 15)));
                }
            }
        }
        if (headerEnd != std::string::npos && request.size() >= headerEnd + 4 + contentLength) break;
    }
    if (headerEnd == std::string::npos) { respond(client, 400, "text/plain", "Malformed request"); return; }
    std::istringstream firstLine(request.substr(0, request.find("\r\n")));
    std::string method, path, version;
    firstLine >> method >> path >> version;
    const std::string body = request.substr(headerEnd + 4, contentLength);

    if (method == "POST" && path == "/api/compile") {
        mini::CompileResult result = mini::compileSource(body);
        respond(client, 200, "application/json", mini::compileResultToJson(result));
        return;
    }
    if (method == "GET" && path == "/api/health") {
        respond(client, 200, "application/json", "{\"status\":\"ok\"}"); return;
    }
    if (method == "GET") {
        std::string relative;
        std::string mime = "text/plain";
        if (path == "/" || path == "/index.html") { relative = "/index.html"; mime = "text/html"; }
        else if (path == "/app.js") { relative = "/app.js"; mime = "application/javascript"; }
        else if (path == "/style.css") { relative = "/style.css"; mime = "text/css"; }
        else { respond(client, 404, "text/plain", "Not found"); return; }
        std::string content = readFile(std::string(MINICPP_WEB_DIR) + relative);
        if (content.empty()) { respond(client, 404, "text/plain", "Frontend file not found. Check MINICPP_WEB_DIR."); return; }
        respond(client, 200, mime, content); return;
    }
    respond(client, 404, "text/plain", "Not found");
}
}

int runHttpServer(int port) {
#ifdef _WIN32
    WSADATA winsockData{};
    if (WSAStartup(MAKEWORD(2, 2), &winsockData) != 0) {
        std::cerr << "WSAStartup failed\n"; return 1;
    }
#endif
    socket_type server = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server == INVALID_SOCKET_VALUE) {
        std::cerr << "Cannot create socket\n";
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    int yes = 1;
#ifdef _WIN32
    ::setsockopt(server, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));
#else
    ::setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#endif
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(static_cast<uint16_t>(port));
    if (::bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        std::cerr << "Cannot bind to 127.0.0.1:" << port << "\n";
        closeSocket(server);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    if (::listen(server, 16) != 0) {
        std::cerr << "Cannot listen on socket\n";
        closeSocket(server);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    std::cout << "MiniCpp is running at http://127.0.0.1:" << port << "\nPress Ctrl+C to stop.\n";
    while (true) {
        socket_type client = ::accept(server, nullptr, nullptr);
        if (client == INVALID_SOCKET_VALUE) continue;
        handleClient(client);
#ifdef _WIN32
        ::shutdown(client, SD_BOTH);
#else
        ::shutdown(client, SHUT_RDWR);
#endif
        closeSocket(client);
    }
    closeSocket(server);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
