#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "minmax.h"
#include "protocol.h"

namespace {

constexpr std::uint16_t kDefaultPort = 5000;

void logError(const std::string& message) {
    std::cerr << "[server] " << message << " (err=" << getSocketError() << ")" << std::endl;
}

bool handleClient(socket_t clientFd) {
    RequestHeader header{};
    if (!recvAll(clientFd, &header, sizeof(header))) {
        logError("failed to read request header");
        return false;
    }

    std::uint32_t rows = fromNetworkU32(header.rows);
    std::uint32_t cols = fromNetworkU32(header.cols);
    std::uint32_t numThreads = fromNetworkU32(header.numThreads);

    if (rows == 0 || cols == 0) {
        logError("invalid matrix dimensions");
        return false;
    }

    std::size_t total = static_cast<std::size_t>(rows) * static_cast<std::size_t>(cols);
    std::vector<std::uint32_t> raw(total);
    if (!recvAll(clientFd, raw.data(), raw.size() * sizeof(std::uint32_t))) {
        logError("failed to read matrix payload");
        return false;
    }

    std::vector<std::int32_t> data(total);
    for (std::size_t i = 0; i < total; ++i) {
        data[i] = fromNetworkI32(raw[i]);
    }

    MinMaxResult result = findMinMaxParallel(data, rows, cols, numThreads);

    ResponseHeader response{};
    response.minValue = static_cast<std::int32_t>(toNetworkI32(result.minValue));
    response.maxValue = static_cast<std::int32_t>(toNetworkI32(result.maxValue));

    if (!sendAll(clientFd, &response, sizeof(response))) {
        logError("failed to send response");
        return false;
    }

    return true;
}

} // namespace

int main(int argc, char** argv) {
    std::uint16_t port = kDefaultPort;
    if (argc >= 2) {
        port = static_cast<std::uint16_t>(std::stoi(argv[1]));
    }

#ifdef _WIN32
    WSADATA wsaData{};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        logError("WSAStartup failed");
        return 1;
    }
#endif

    socket_t serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd < 0) {
        logError("socket creation failed");
        return 1;
    }

    int opt = 1;
    if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&opt), sizeof(opt)) < 0) {
        logError("setsockopt failed");
        closeSocket(serverFd);
        return 1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(serverFd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        logError("bind failed");
        closeSocket(serverFd);
        return 1;
    }

    if (listen(serverFd, 8) < 0) {
        logError("listen failed");
        closeSocket(serverFd);
        return 1;
    }

    std::cout << "[server] listening on port " << port << std::endl;

    while (true) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        socket_t clientFd = accept(serverFd, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
        if (clientFd < 0) {
            logError("accept failed");
            continue;
        }

        bool ok = handleClient(clientFd);
        if (!ok) {
            std::cerr << "[server] handled client with errors" << std::endl;
        }
        closeSocket(clientFd);
    }

    closeSocket(serverFd);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
