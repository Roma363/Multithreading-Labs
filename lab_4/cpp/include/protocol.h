#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>


constexpr std::uint32_t kProtocolMagic = 0x4D4D5831; // "MMX1"
constexpr std::uint16_t kProtocolVersion = 1;
constexpr std::uint32_t kMaxPayloadBytes = 256 * 1024 * 1024;

enum MessageType : std::uint16_t {
    MSG_DATA = 1,
    MSG_START = 2,
    MSG_STATUS = 3,
    MSG_DATA_OK = 101,
    MSG_START_OK = 102,
    MSG_STATUS_RESP = 103,
    MSG_ERROR = 200
};

enum StatusCode : std::uint32_t {
    STATUS_IDLE = 0,
    STATUS_READY = 1,
    STATUS_RUNNING = 2,
    STATUS_DONE = 3,
    STATUS_ERROR = 4
};

enum ErrorCode : std::uint32_t {
    ERR_NONE = 0,
    ERR_INVALID_PAYLOAD = 1,
    ERR_NO_DATA = 2,
    ERR_ALREADY_RUNNING = 3,
    ERR_INTERNAL = 4
};

struct MessageHeader {
    std::uint32_t magic;
    std::uint16_t version;
    std::uint16_t type;
    std::uint32_t length;
};

inline std::uint32_t toNetworkU32(std::uint32_t value) {
    return htonl(value);
}

inline std::uint32_t fromNetworkU32(std::uint32_t value) {
    return ntohl(value);
}

inline std::uint16_t toNetworkU16(std::uint16_t value) {
    return htons(value);
}

inline std::uint16_t fromNetworkU16(std::uint16_t value) {
    return ntohs(value);
}

inline std::uint32_t toNetworkI32(std::int32_t value) {
    return htonl(static_cast<std::uint32_t>(value));
}

inline std::int32_t fromNetworkI32(std::uint32_t value) {
    return static_cast<std::int32_t>(ntohl(value));
}

using socket_t = SOCKET;
inline int closeSocket(socket_t socketFd) {
    return closesocket(socketFd);
}
inline int getSocketError() {
    return WSAGetLastError();
}

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

// Додає беззнакове 32-бітне число до буфера в мережевому форматі (big-endian)
inline void appendU32(std::vector<std::uint8_t>& buffer, std::uint32_t value) {
    std::uint32_t net = toNetworkU32(value);
    const std::uint8_t* ptr = reinterpret_cast<const std::uint8_t*>(&net);
    buffer.insert(buffer.end(), ptr, ptr + sizeof(net));
}

// Додає знакове 32-бітне число до буфера в мережевому форматі (big-endian)
inline void appendI32(std::vector<std::uint8_t>& buffer, std::int32_t value) {
    std::uint32_t net = toNetworkI32(value);
    const std::uint8_t* ptr = reinterpret_cast<const std::uint8_t*>(&net);
    buffer.insert(buffer.end(), ptr, ptr + sizeof(net));
}

// Читає беззнакове 32-бітне число з буфера, конвертує з мережевого формату
inline bool readU32(const std::vector<std::uint8_t>& buffer, std::size_t& offset, std::uint32_t& value) {
    if (offset + sizeof(std::uint32_t) > buffer.size()) {
        return false;
    }
    std::uint32_t net = 0;
    std::memcpy(&net, buffer.data() + offset, sizeof(net));
    offset += sizeof(net);
    value = fromNetworkU32(net);
    return true;
}

// Читає знакове 32-бітне число з буфера, конвертує з мережевого формату
inline bool readI32(const std::vector<std::uint8_t>& buffer, std::size_t& offset, std::int32_t& value) {
    if (offset + sizeof(std::uint32_t) > buffer.size()) {
        return false;
    }
    std::uint32_t net = 0;
    std::memcpy(&net, buffer.data() + offset, sizeof(net));
    offset += sizeof(net);
    value = fromNetworkI32(net);
    return true;
}

// Відправляє повідомлення з заголовком та корисним навантаженням через сокет
inline bool sendMessage(socket_t socketFd, std::uint16_t type, const void* payload, std::uint32_t length) {
    // Готує заголовок з магічним числом, версією, типом та розміром
    MessageHeader header{};
    header.magic = kProtocolMagic;
    header.version = kProtocolVersion;
    header.type = type;
    header.length = length;

    // Конвертує заголовок в мережевий формат (big-endian)
    MessageHeader netHeader{};
    netHeader.magic = toNetworkU32(header.magic);
    netHeader.version = toNetworkU16(header.version);
    netHeader.type = toNetworkU16(header.type);
    netHeader.length = toNetworkU32(header.length);

    // Відправляє заголовок, потім корисне навантаження
    if (!sendAll(socketFd, &netHeader, sizeof(netHeader))) {
        return false;
    }
    if (length == 0) {
        return true;
    }
    return sendAll(socketFd, payload, length);
}

// Отримує повідомлення з сокета, валідує заголовок та читає корисне навантаження
inline bool recvMessage(socket_t socketFd, MessageHeader& header, std::vector<std::uint8_t>& payload) {
    // Приймає заголовок з сокета
    MessageHeader netHeader{};
    if (!recvAll(socketFd, &netHeader, sizeof(netHeader))) {
        return false;
    }

    // Конвертує заголовок зі мережевого формату
    header.magic = fromNetworkU32(netHeader.magic);
    header.version = fromNetworkU16(netHeader.version);
    header.type = fromNetworkU16(netHeader.type);
    header.length = fromNetworkU32(netHeader.length);

    // Валідує магічне число, версію та розмір корисного навантаження
    if (header.magic != kProtocolMagic || header.version != kProtocolVersion) {
        return false;
    }
    if (header.length > kMaxPayloadBytes) {
        return false;
    }

    // Приймає корисне навантаження зі змінним розміром
    payload.clear();
    if (header.length == 0) {
        return true;
    }
    payload.resize(header.length);
    return recvAll(socketFd, payload.data(), header.length);
}