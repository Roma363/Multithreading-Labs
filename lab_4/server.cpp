#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "minmax.h"
#include "protocol.h"

namespace {

constexpr std::uint16_t kDefaultPort = 5000;

void logError(const std::string& message) {
    std::cerr << "[server] " << message << " (err=" << getSocketError() << ")" << std::endl;
}

struct ClientState {
    std::mutex mutex;
    std::vector<std::int32_t> data;
    std::uint32_t rows = 0;
    std::uint32_t cols = 0;
    std::uint32_t numThreads = 0;
    StatusCode status = STATUS_IDLE;
    ErrorCode error = ERR_NONE;
    std::int32_t minValue = 0;
    std::int32_t maxValue = 0;
    bool hasData = false;
    std::thread worker;
};

bool sendError(socket_t clientFd, ErrorCode code) {
    std::vector<std::uint8_t> payload;
    appendU32(payload, static_cast<std::uint32_t>(code));
    return sendMessage(clientFd, MSG_ERROR, payload.data(), static_cast<std::uint32_t>(payload.size()));
}

bool sendStatus(socket_t clientFd, StatusCode status, std::int32_t minValue, std::int32_t maxValue, ErrorCode error) {
    std::vector<std::uint8_t> payload;
    appendU32(payload, status);
    appendI32(payload, minValue);
    appendI32(payload, maxValue);
    appendU32(payload, static_cast<std::uint32_t>(error));
    return sendMessage(clientFd, MSG_STATUS_RESP, payload.data(), static_cast<std::uint32_t>(payload.size()));
}

bool handleDataCommand(ClientState& state, socket_t clientFd, const std::vector<std::uint8_t>& payload) {
    std::size_t offset = 0;
    std::uint32_t rows = 0;
    std::uint32_t cols = 0;
    std::uint32_t numThreads = 0;
    if (!readU32(payload, offset, rows) || !readU32(payload, offset, cols) || !readU32(payload, offset, numThreads)) {
        return sendError(clientFd, ERR_INVALID_PAYLOAD);
    }

    if (rows == 0 || cols == 0) {
        return sendError(clientFd, ERR_INVALID_PAYLOAD);
    }

    std::size_t total = static_cast<std::size_t>(rows) * static_cast<std::size_t>(cols);
    std::size_t expectedSize = sizeof(std::uint32_t) * 3 + total * sizeof(std::int32_t);
    if (payload.size() != expectedSize) {
        return sendError(clientFd, ERR_INVALID_PAYLOAD);
    }

    std::vector<std::int32_t> data(total);
    for (std::size_t i = 0; i < total; ++i) {
        std::int32_t value = 0;
        if (!readI32(payload, offset, value)) {
            return sendError(clientFd, ERR_INVALID_PAYLOAD);
        }
        data[i] = value;
    }

    std::lock_guard<std::mutex> guard(state.mutex);
    if (state.status == STATUS_RUNNING) {
        return sendError(clientFd, ERR_ALREADY_RUNNING);
    }
    if (state.worker.joinable()) {
        state.worker.join();
    }

    state.data = std::move(data);
    state.rows = rows;
    state.cols = cols;
    state.numThreads = numThreads;
    state.status = STATUS_READY;
    state.error = ERR_NONE;
    state.hasData = true;

    return sendMessage(clientFd, MSG_DATA_OK, nullptr, 0);
}

bool handleStartCommand(ClientState& state, socket_t clientFd) {
    std::lock_guard<std::mutex> guard(state.mutex);
    if (!state.hasData) {
        return sendError(clientFd, ERR_NO_DATA);
    }
    if (state.status == STATUS_RUNNING) {
        return sendError(clientFd, ERR_ALREADY_RUNNING);
    }

    state.status = STATUS_RUNNING;
    state.error = ERR_NONE;
    std::vector<std::int32_t> dataCopy = state.data;
    std::uint32_t rows = state.rows;
    std::uint32_t cols = state.cols;
    std::uint32_t numThreads = state.numThreads;

    auto dataPtr = std::make_shared<std::vector<std::int32_t>>(std::move(dataCopy));
    state.worker = std::thread([&state, dataPtr, rows, cols, numThreads]() {
        try {
            MinMaxResult result = findMinMaxParallel(*dataPtr, rows, cols, numThreads);
            std::lock_guard<std::mutex> lock(state.mutex);
            state.minValue = result.minValue;
            state.maxValue = result.maxValue;
            state.status = STATUS_DONE;
            state.error = ERR_NONE;
        } catch (...) {
            std::lock_guard<std::mutex> lock(state.mutex);
            state.status = STATUS_ERROR;
            state.error = ERR_INTERNAL;
        }
    });

    return sendMessage(clientFd, MSG_START_OK, nullptr, 0);
}

bool handleStatusCommand(ClientState& state, socket_t clientFd) {
    std::lock_guard<std::mutex> guard(state.mutex);
    std::int32_t minValue = (state.status == STATUS_DONE) ? state.minValue : 0;
    std::int32_t maxValue = (state.status == STATUS_DONE) ? state.maxValue : 0;
    return sendStatus(clientFd, state.status, minValue, maxValue, state.error);
}

bool handleClient(socket_t clientFd) {
    ClientState state;
    while (true) {
        MessageHeader header{};
        std::vector<std::uint8_t> payload;
        if (!recvMessage(clientFd, header, payload)) {
            break;
        }

        bool ok = false;
        switch (header.type) {
            case MSG_DATA:
                ok = handleDataCommand(state, clientFd, payload);
                break;
            case MSG_START:
                ok = handleStartCommand(state, clientFd);
                break;
            case MSG_STATUS:
                ok = handleStatusCommand(state, clientFd);
                break;
            default:
                ok = sendError(clientFd, ERR_INVALID_PAYLOAD);
                break;
        }

        if (!ok) {
            break;
        }
    }

    if (state.worker.joinable()) {
        state.worker.join();
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
