#include <cstdint>
#include <chrono>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "protocol.h"

namespace {

constexpr std::uint16_t kDefaultPort = 5000;

void printUsage(const char* exeName) {
    std::cout << "Usage: " << exeName << " (no arguments)\n";
    std::cout << "Defaults: host=127.0.0.1 rows=100 cols=100 numThreads=4 port=5000\n";
}

std::vector<std::int32_t> generateMatrix(std::size_t rows, std::size_t cols, std::int32_t minValue, std::int32_t maxValue) {
    std::vector<std::int32_t> data(rows * cols);
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<std::int32_t> dist(minValue, maxValue);

    for (auto& value : data) {
        value = dist(rng);
    }

    return data;
}

bool recvExpected(socket_t sockFd, std::uint16_t expectedType, std::vector<std::uint8_t>& payload) {
    MessageHeader header{};
    if (!recvMessage(sockFd, header, payload)) {
        return false;
    }
    if (header.type == MSG_ERROR) {
        std::size_t offset = 0;
        std::uint32_t code = 0;
        if (readU32(payload, offset, code)) {
            std::cerr << "[client] server error code: " << code << std::endl;
        } else {
            std::cerr << "[client] server error" << std::endl;
        }
        return false;
    }
    if (header.type != expectedType) {
        std::cerr << "[client] unexpected response type: " << header.type << std::endl;
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 1) {
        printUsage(argv[0]);
        return 1;
    }

    std::string host = "127.0.0.1";
    std::uint32_t rows = 100;
    std::uint32_t cols = 100;
    std::uint32_t numThreads = 4;
    std::uint16_t port = kDefaultPort;

    if (rows == 0 || cols == 0) {
        std::cerr << "[client] rows and cols must be > 0" << std::endl;
        return 1;
    }

#ifdef _WIN32
    WSADATA wsaData{};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "[client] WSAStartup failed" << std::endl;
        return 1;
    }
#endif

    std::vector<std::int32_t> matrix = generateMatrix(rows, cols, -1000, 1000);

    socket_t sockFd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockFd < 0) {
        std::cerr << "[client] socket creation failed" << std::endl;
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr) <= 0) {
        std::cerr << "[client] invalid server address" << std::endl;
        closeSocket(sockFd);
    #ifdef _WIN32
        WSACleanup();
    #endif
        return 1;
    }

    if (connect(sockFd, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        std::cerr << "[client] connect failed" << std::endl;
        closeSocket(sockFd);
    #ifdef _WIN32
        WSACleanup();
    #endif
        return 1;
    }

    std::vector<std::uint8_t> dataPayload;
    dataPayload.reserve(sizeof(std::uint32_t) * 3 + matrix.size() * sizeof(std::int32_t));
    appendU32(dataPayload, rows);
    appendU32(dataPayload, cols);
    appendU32(dataPayload, numThreads);
    for (const auto value : matrix) {
        appendI32(dataPayload, value);
    }

    if (!sendMessage(sockFd, MSG_DATA, dataPayload.data(), static_cast<std::uint32_t>(dataPayload.size()))) {
        std::cerr << "[client] failed to send DATA" << std::endl;
        closeSocket(sockFd);
    #ifdef _WIN32
        WSACleanup();
    #endif
        return 1;
    }

    std::vector<std::uint8_t> payload;
    if (!recvExpected(sockFd, MSG_DATA_OK, payload)) {
        closeSocket(sockFd);
    #ifdef _WIN32
        WSACleanup();
    #endif
        return 1;
    }

    if (!sendMessage(sockFd, MSG_START, nullptr, 0)) {
        std::cerr << "[client] failed to send START" << std::endl;
        closeSocket(sockFd);
    #ifdef _WIN32
        WSACleanup();
    #endif
        return 1;
    }

    if (!recvExpected(sockFd, MSG_START_OK, payload)) {
        closeSocket(sockFd);
    #ifdef _WIN32
        WSACleanup();
    #endif
        return 1;
    }

    while (true) {
        if (!sendMessage(sockFd, MSG_STATUS, nullptr, 0)) {
            std::cerr << "[client] failed to send STATUS" << std::endl;
            break;
        }

        if (!recvExpected(sockFd, MSG_STATUS_RESP, payload)) {
            break;
        }

        std::size_t offset = 0;
        std::uint32_t status = 0;
        std::int32_t minValue = 0;
        std::int32_t maxValue = 0;
        std::uint32_t error = 0;
        if (!readU32(payload, offset, status) || !readI32(payload, offset, minValue) ||
            !readI32(payload, offset, maxValue) || !readU32(payload, offset, error)) {
            std::cerr << "[client] invalid STATUS response" << std::endl;
            break;
        }

        if (status == STATUS_DONE) {
            std::cout << "[client] result: min=" << minValue << ", max=" << maxValue << std::endl;
            break;
        }

        if (status == STATUS_ERROR) {
            std::cerr << "[client] server reported error: " << error << std::endl;
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

#ifdef _WIN32
    closeSocket(sockFd);
    WSACleanup();
#else
    closeSocket(sockFd);
#endif
    return 0;
}
