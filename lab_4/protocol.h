#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

struct RequestHeader {
    std::uint32_t rows;
    std::uint32_t cols;
    std::uint32_t numThreads;
};

struct ResponseHeader {
    std::int32_t minValue;
    std::int32_t maxValue;
};

inline std::uint32_t toNetworkU32(std::uint32_t value) {
    return htonl(value);
}

inline std::uint32_t fromNetworkU32(std::uint32_t value) {
    return ntohl(value);
}

inline std::uint32_t toNetworkI32(std::int32_t value) {
    return htonl(static_cast<std::uint32_t>(value));
}

inline std::int32_t fromNetworkI32(std::uint32_t value) {
    return static_cast<std::int32_t>(ntohl(value));
}

#ifdef _WIN32
using socket_t = SOCKET;
inline int closeSocket(socket_t socketFd) {
    return closesocket(socketFd);
}
inline int getSocketError() {
    return WSAGetLastError();
}
#else
using socket_t = int;
inline int closeSocket(socket_t socketFd) {
    return close(socketFd);
}
inline int getSocketError() {
    return errno;
}
#endif

inline bool sendAll(socket_t socketFd, const void* data, std::size_t size) {
    const std::uint8_t* buffer = static_cast<const std::uint8_t*>(data);
    std::size_t sent = 0;
    while (sent < size) {
        int result = send(socketFd, reinterpret_cast<const char*>(buffer + sent),
                          static_cast<int>(size - sent), 0);
        if (result <= 0) {
            return false;
        }
        sent += static_cast<std::size_t>(result);
    }
    return true;
}

inline bool recvAll(socket_t socketFd, void* data, std::size_t size) {
    std::uint8_t* buffer = static_cast<std::uint8_t*>(data);
    std::size_t received = 0;
    while (received < size) {
        int result = recv(socketFd, reinterpret_cast<char*>(buffer + received),
                          static_cast<int>(size - received), 0);
        if (result <= 0) {
            return false;
        }
        received += static_cast<std::size_t>(result);
    }
    return true;
}
