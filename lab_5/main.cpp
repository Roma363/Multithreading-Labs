// Збільшуємо ліміт одночасних підключень для Locust
#ifndef FD_SETSIZE
#define FD_SETSIZE 1024
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <deque>
#include <filesystem>
#include <cctype>

// Вказуємо компілятору підключити бібліотеку сокетів Windows
#pragma comment(lib, "ws2_32.lib")

namespace {

constexpr int kDefaultPort = 8080;
constexpr size_t kMaxRequestSize = 8192;
constexpr size_t kBufferSize = 4096;

std::string to_lower(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

std::string get_content_type(const std::filesystem::path& path) {
    auto ext = to_lower(path.extension().string());
    if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
    if (ext == ".txt") return "text/plain; charset=utf-8";
    if (ext == ".css") return "text/css; charset=utf-8";
    if (ext == ".js") return "application/javascript; charset=utf-8";
    return "application/octet-stream";
}

struct HttpRequest {
    std::string method;
    std::string target;
    std::string version;
    std::map<std::string, std::string> headers;
};

bool parse_request(const std::string& raw, HttpRequest& request) {
    std::istringstream stream(raw);
    std::string line;
    if (!std::getline(stream, line)) return false;
    if (!line.empty() && line.back() == '\r') line.pop_back();

    std::istringstream line_stream(line);
    if (!(line_stream >> request.method >> request.target >> request.version)) return false;

    while (std::getline(stream, line)) {
        if (line == "\r" || line.empty()) break;
        if (!line.empty() && line.back() == '\r') line.pop_back();

        auto colon_pos = line.find(':');
        if (colon_pos == std::string::npos) continue;

        std::string name = to_lower(line.substr(0, colon_pos));
        std::string value = line.substr(colon_pos + 1);
        while (!value.empty() && value.front() == ' ') value.erase(value.begin());
        request.headers[name] = value;
    }
    return true;
}

std::string build_response(int status_code, const std::string& status_text,
                           const std::string& body, const std::string& content_type) {
    std::ostringstream response;
    response << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";
    response << "Content-Length: " << body.size() << "\r\n";
    response << "Content-Type: " << content_type << "\r\n";
    response << "Connection: close\r\n\r\n";
    response << body;
    return response.str();
}

std::string sanitize_target(std::string target) {
    auto query_pos = target.find('?');
    if (query_pos != std::string::npos) target = target.substr(0, query_pos);
    if (target.empty() || target[0] != '/') target = "/";
    return target;
}

struct ClientState {
    std::string request_buffer;
    std::deque<std::string> response_queue;
    bool request_complete = false;
    size_t response_sent = 0;

    bool is_request_complete() const {
        return request_buffer.find("\r\n\r\n") != std::string::npos;
    }
};

bool set_nonblocking(SOCKET fd) {
    u_long mode = 1;
    return ioctlsocket(fd, FIONBIO, &mode) == 0;
}

void handle_client_request(SOCKET client_fd, ClientState& state, const std::filesystem::path& root) {
    HttpRequest request;
    if (!parse_request(state.request_buffer, request)) {
        state.response_queue.push_back(build_response(400, "Bad Request", "Bad Request", "text/plain; charset=utf-8"));
        return;
    }

    if (request.method != "GET") {
        state.response_queue.push_back(build_response(405, "Method Not Allowed", "Method Not Allowed", "text/plain; charset=utf-8"));
        return;
    }

    std::string target = sanitize_target(request.target);
    if (target == "/") target = "/index.html";

    if (target.find("..") != std::string::npos) {
        state.response_queue.push_back(build_response(400, "Bad Request", "Bad Request", "text/plain; charset=utf-8"));
        return;
    }

    std::filesystem::path file_path = root / target.substr(1);
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        state.response_queue.push_back(build_response(404, "Not Found", "Not Found", "text/plain; charset=utf-8"));
        return;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    state.response_queue.push_back(build_response(200, "OK", buffer.str(), get_content_type(file_path)));
}

}  // namespace

int main(int argc, char* argv[]) {
    int port = kDefaultPort;
    std::filesystem::path root = "www";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind("--port=", 0) == 0) {
            try { port = std::stoi(arg.substr(7)); } catch (...) {}
        } else if (arg.rfind("--root=", 0) == 0) {
            root = arg.substr(7);
        }
    }

    // Ініціалізація Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }

    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd == INVALID_SOCKET) {
        std::cerr << "socket failed: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    if (!set_nonblocking(server_fd)) {
        std::cerr << "ioctlsocket failed: " << WSAGetLastError() << "\n";
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(server_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
        std::cerr << "bind failed: " << WSAGetLastError() << "\n";
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    if (listen(server_fd, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "listen failed: " << WSAGetLastError() << "\n";
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    std::map<SOCKET, ClientState> clients;
    std::cout << "Non-blocking HTTP server (Windows) listening on port " << port << " (root: " << root << ")\n";

    while (true) {
        fd_set read_fds, write_fds;
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);

        FD_SET(server_fd, &read_fds);

        for (const auto& pair : clients) {
            SOCKET client_fd = pair.first;
            const ClientState& state = pair.second;

            if (!state.request_complete) {
                FD_SET(client_fd, &read_fds);
            }
            if (state.request_complete && !state.response_queue.empty()) {
                FD_SET(client_fd, &write_fds);
            }
        }

        // Очікуємо на активність
        timeval timeout = {1, 0}; // 1 секунда таймаут
        int activity = select(0, &read_fds, &write_fds, nullptr, &timeout);

        if (activity == SOCKET_ERROR) {
            std::cerr << "select failed: " << WSAGetLastError() << "\n";
            break;
        }

        // 1. Приймаємо нові підключення
        if (FD_ISSET(server_fd, &read_fds)) {
            while (true) {
                sockaddr_in client_addr{};
                int client_len = sizeof(client_addr);
                SOCKET client_fd = accept(server_fd, reinterpret_cast<sockaddr*>(&client_addr), &client_len);

                if (client_fd == INVALID_SOCKET) {
                    if (WSAGetLastError() != WSAEWOULDBLOCK) {
                        std::cerr << "accept failed: " << WSAGetLastError() << "\n";
                    }
                    break;
                }

                set_nonblocking(client_fd);
                clients[client_fd] = ClientState{};
            }
        }

        // 2. Обробляємо існуючих клієєнтів
        for (auto it = clients.begin(); it != clients.end(); ) {
            SOCKET client_fd = it->first;
            ClientState& state = it->second;
            bool close_connection = false;

            // Читання даних від клієнта
            if (FD_ISSET(client_fd, &read_fds) && !state.request_complete) {
                std::vector<char> buffer(kBufferSize);
                int bytes = recv(client_fd, buffer.data(), static_cast<int>(buffer.size()), 0);

                if (bytes > 0) {
                    state.request_buffer.append(buffer.data(), bytes);
                    if (state.request_buffer.size() > kMaxRequestSize) {
                        state.response_queue.push_back(build_response(413, "Payload Too Large", "Payload Too Large", "text/plain; charset=utf-8"));
                        state.request_complete = true;
                    } else if (state.is_request_complete()) {
                        handle_client_request(client_fd, state, root);
                        state.request_complete = true;
                    }
                } else if (bytes == 0) {
                    close_connection = true;
                } else {
                    if (WSAGetLastError() != WSAEWOULDBLOCK) {
                        close_connection = true;
                    }
                }
            }

            // Відправка даних клієнту
            if (!close_connection && FD_ISSET(client_fd, &write_fds) && state.request_complete && !state.response_queue.empty()) {
                auto& response = state.response_queue.front();
                int sent = send(client_fd, response.data() + state.response_sent, static_cast<int>(response.size() - state.response_sent), 0);

                if (sent > 0) {
                    state.response_sent += sent;
                    if (state.response_sent >= response.size()) {
                        state.response_queue.pop_front();
                        state.response_sent = 0;
                        if (state.response_queue.empty()) {
                            close_connection = true; // Connection: close реалізація
                        }
                    }
                } else {
                    if (WSAGetLastError() != WSAEWOULDBLOCK) {
                        close_connection = true;
                    }
                }
            }

            if (close_connection) {
                closesocket(client_fd);
                it = clients.erase(it);
            } else {
                ++it;
            }
        }
    }

    closesocket(server_fd);
    WSACleanup();
    return 0;
}