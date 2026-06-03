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

// Логування помилки з кодом сокета
void logError(const std::string& message) {
    std::cerr << "[server] " << message << " (err=" << getSocketError() << ")" << std::endl;
}

// Стан клієнта: матриця, параметри, статус та результати обробки
struct ClientState {
    std::mutex mutex;
    std::vector<std::int32_t> data;  // Матриця даних
    std::uint32_t rows = 0;           // Кількість рядків
    std::uint32_t cols = 0;           // Кількість стовпців
    std::uint32_t numThreads = 0;     // Кількість потоків для обробки
    StatusCode status = STATUS_IDLE;  // Поточний статус обробки
    ErrorCode error = ERR_NONE;       // Код помилки
    std::int32_t minValue = 0;        // Мінімальне значення
    std::int32_t maxValue = 0;        // Максимальне значення
    bool hasData = false;             // Флаг наявності даних
    std::thread worker;               // Робочий потік для обробки
};

// Відправляє повідомлення про помилку з кодом
bool sendError(socket_t clientFd, ErrorCode code) {
    std::vector<std::uint8_t> payload;
    appendU32(payload, static_cast<std::uint32_t>(code));
    return sendMessage(clientFd, MSG_ERROR, payload.data(), static_cast<std::uint32_t>(payload.size()));
}

// Відправляє статус обробки з поточними мін/макс значеннями та кодом помилки
bool sendStatus(socket_t clientFd, StatusCode status, std::int32_t minValue, std::int32_t maxValue, ErrorCode error) {
    std::vector<std::uint8_t> payload;
    appendU32(payload, status);
    appendI32(payload, minValue);
    appendI32(payload, maxValue);
    appendU32(payload, static_cast<std::uint32_t>(error));
    return sendMessage(clientFd, MSG_STATUS_RESP, payload.data(), static_cast<std::uint32_t>(payload.size()));
}

// Обробляє команду отримання матриці від клієнта
bool handleDataCommand(ClientState& state, socket_t clientFd, const std::vector<std::uint8_t>& payload) {
    // Парсить заголовок: розміри матриці та кількість потоків
    std::size_t offset = 0;
    std::uint32_t rows = 0;
    std::uint32_t cols = 0;
    std::uint32_t numThreads = 0;
    if (!readU32(payload, offset, rows) || !readU32(payload, offset, cols) || !readU32(payload, offset, numThreads)) {
        return sendError(clientFd, ERR_INVALID_PAYLOAD);
    }

    // Перевіряє валідність розмірів
    if (rows == 0 || cols == 0) {
        return sendError(clientFd, ERR_INVALID_PAYLOAD);
    }

    // Перевіряє, чи розмір корисного навантаження збігається з очікуваним
    std::size_t total = static_cast<std::size_t>(rows) * static_cast<std::size_t>(cols);
    std::size_t expectedSize = sizeof(std::uint32_t) * 3 + total * sizeof(std::int32_t);
    if (payload.size() != expectedSize) {
        return sendError(clientFd, ERR_INVALID_PAYLOAD);
    }

    // Читає елементи матриці з корисного навантаження
    std::vector<std::int32_t> data(total);
    for (std::size_t i = 0; i < total; ++i) {
        std::int32_t value = 0;
        if (!readI32(payload, offset, value)) {
            return sendError(clientFd, ERR_INVALID_PAYLOAD);
        }
        data[i] = value;
    }

    // Зберігає матрицю в стані клієнта, дочекавшись завершення попередньої обробки
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

    // Відправляє підтвердження прийому матриці
    return sendMessage(clientFd, MSG_DATA_OK, nullptr, 0);
}

// Обробляє команду запуску обробки матриці
bool handleStartCommand(ClientState& state, socket_t clientFd) {
    std::lock_guard<std::mutex> guard(state.mutex);
    // Перевіряє, чи матриця була передана та чи не запущена вже обробка
    if (!state.hasData) {
        return sendError(clientFd, ERR_NO_DATA);
    }
    if (state.status == STATUS_RUNNING) {
        return sendError(clientFd, ERR_ALREADY_RUNNING);
    }

    // Задає статус запущено та копіює дані для робочого потоку
    state.status = STATUS_RUNNING;
    state.error = ERR_NONE;
    std::vector<std::int32_t> dataCopy = state.data;
    std::uint32_t rows = state.rows;
    std::uint32_t cols = state.cols;
    std::uint32_t numThreads = state.numThreads;

    // Створює робочий потік для обробки матриці з пошуком мін/макс
    auto dataPtr = std::make_shared<std::vector<std::int32_t>>(std::move(dataCopy));
    state.worker = std::thread([&state, dataPtr, rows, cols, numThreads]() {
        try {
            // Виконує паралельний пошук мінімуму та максимуму
            MinMaxResult result = findMinMaxParallel(*dataPtr, rows, cols, numThreads);
            // Зберігає результати та задає статус завершено
            std::lock_guard<std::mutex> lock(state.mutex);
            state.minValue = result.minValue;
            state.maxValue = result.maxValue;
            state.status = STATUS_DONE;
            state.error = ERR_NONE;
        } catch (...) {
            // Обробляє внутрішні помилки
            std::lock_guard<std::mutex> lock(state.mutex);
            state.status = STATUS_ERROR;
            state.error = ERR_INTERNAL;
        }
    });

    // Відправляє підтвердження запуску обробки
    return sendMessage(clientFd, MSG_START_OK, nullptr, 0);
}

// Обробляє запит статусу обробки матриці
bool handleStatusCommand(ClientState& state, socket_t clientFd) {
    std::lock_guard<std::mutex> guard(state.mutex);
    // Відправляє статус та результати (мін/макс) якщо обробка завершена
    std::int32_t minValue = (state.status == STATUS_DONE) ? state.minValue : 0;
    std::int32_t maxValue = (state.status == STATUS_DONE) ? state.maxValue : 0;
    return sendStatus(clientFd, state.status, minValue, maxValue, state.error);
}

// Основний цикл обробки запитів від одного клієнта
void handleClient(socket_t clientFd) {
    ClientState state;
    while (true) {
        // Отримує повідомлення від клієнта
        MessageHeader header{};
        std::vector<std::uint8_t> payload;
        if (!recvMessage(clientFd, header, payload)) {
            break;
        }

        // Обробляє команду та відправляє відповідь
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

    // Чекає завершення робочого потоку та закриває сокет
    if (state.worker.joinable()) {
        state.worker.join();
    }
    closeSocket(clientFd);
}

} // namespace

int main(int argc, char** argv) {
    // Встановлює порт за замовчуванням або від аргументів командного рядка
    std::uint16_t port = kDefaultPort;
    if (argc >= 2) {
        port = static_cast<std::uint16_t>(std::stoi(argv[1]));
    }

    // Ініціалізує Windows Sockets
    WSADATA wsaData{};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        logError("WSAStartup failed");
        return 1;
    }

    // Створює серверний сокет для TCP
    socket_t serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd == INVALID_SOCKET) {
        logError("socket creation failed");
        WSACleanup();
        return 1;
    }

    // Дозволяє повторне використання адреси для швидкого перезапуску
    int opt = 1;
    if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&opt), sizeof(opt)) == SOCKET_ERROR) {
        logError("setsockopt failed");
        closeSocket(serverFd);
        WSACleanup();
        return 1;
    }

    // Біндує сокет до всіх інтерфейсів на певному порту
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(serverFd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
        logError("bind failed");
        closeSocket(serverFd);
        WSACleanup();
        return 1;
    }

    // Переводить сокет в режим прослуховування з черзою на 8 з'єднань
    if (listen(serverFd, 8) == SOCKET_ERROR) {
        logError("listen failed");
        closeSocket(serverFd);
        WSACleanup();
        return 1;
    }

    std::cout << "[server] listening on port " << port << std::endl;

    // Циклічно приймає з'єднання та обробляє кожного клієнта в окремому потоці
    while (true) {
        sockaddr_in clientAddr{};
        int clientLen = sizeof(clientAddr);
        socket_t clientFd = accept(serverFd, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
        if (clientFd == INVALID_SOCKET) {
            logError("accept failed");
            continue;
        }

        // Створює новий потік для обробки клієнта та відпускає його
        std::thread clientThread(handleClient, clientFd);
        clientThread.detach();
    }

    // Очищення ресурсів (ця частина ніколи не виконується при нормальній роботі)
    closeSocket(serverFd);
    WSACleanup();
    return 0;
}
