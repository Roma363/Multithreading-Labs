#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "protocol.h"

namespace {

constexpr std::uint16_t kDefaultPort = 5000;

void printUsage(const char* exeName) {
    std::cout << "Usage: " << exeName << " <host> <rows> <cols> [numThreads] [port]\n";
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

} // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        printUsage(argv[0]);
        return 1;
    }

    std::string host = argv[1];
    std::uint32_t rows = static_cast<std::uint32_t>(std::stoul(argv[2]));
    std::uint32_t cols = static_cast<std::uint32_t>(std::stoul(argv[3]));
    std::uint32_t numThreads = (argc >= 5) ? static_cast<std::uint32_t>(std::stoul(argv[4])) : 4;
    std::uint16_t port = (argc >= 6) ? static_cast<std::uint16_t>(std::stoul(argv[5])) : kDefaultPort;

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

    RequestHeader header{};
    header.rows = toNetworkU32(rows);
    header.cols = toNetworkU32(cols);
    header.numThreads = toNetworkU32(numThreads);

    if (!sendAll(sockFd, &header, sizeof(header))) {
        std::cerr << "[client] failed to send header" << std::endl;
        closeSocket(sockFd);
    #ifdef _WIN32
        WSACleanup();
    #endif
        return 1;
    }

    std::vector<std::uint32_t> raw(matrix.size());
    for (std::size_t i = 0; i < matrix.size(); ++i) {
        raw[i] = toNetworkI32(matrix[i]);
    }

    if (!sendAll(sockFd, raw.data(), raw.size() * sizeof(std::uint32_t))) {
        std::cerr << "[client] failed to send matrix payload" << std::endl;
        closeSocket(sockFd);
    #ifdef _WIN32
        WSACleanup();
    #endif
        return 1;
    }

    ResponseHeader response{};
    if (!recvAll(sockFd, &response, sizeof(response))) {
        std::cerr << "[client] failed to receive response" << std::endl;
        closeSocket(sockFd);
    #ifdef _WIN32
        WSACleanup();
    #endif
        return 1;
    }

    std::int32_t minValue = fromNetworkI32(static_cast<std::uint32_t>(response.minValue));
    std::int32_t maxValue = fromNetworkI32(static_cast<std::uint32_t>(response.maxValue));

    std::cout << "[client] result: min=" << minValue << ", max=" << maxValue << std::endl;

#ifdef _WIN32
    closeSocket(sockFd);
    WSACleanup();
#else
    closeSocket(sockFd);
#endif
    return 0;
}
