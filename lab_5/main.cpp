#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr int kDefaultPort = 8080;
constexpr size_t kMaxRequestSize = 8192;
constexpr int kMaxEvents = 256;
constexpr size_t kBufferSize = 4096;

std::string to_lower(std::string value) {
    for (char &ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

std::string get_content_type(const std::filesystem::path &path) {
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

bool parse_request(const std::string &raw, HttpRequest &request) {
    std::istringstream stream(raw);
    std::string line;
    if (!std::getline(stream, line)) {
        return false;
    }
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    std::istringstream line_stream(line);
    if (!(line_stream >> request.method >> request.target >> request.version)) {
        return false;
    }

    while (std::getline(stream, line)) {
        if (line == "\r" || line.empty()) {
            break;
        }
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        auto colon_pos = line.find(':');
        if (colon_pos == std::string::npos) {
            continue;
        }
        std::string name = to_lower(line.substr(0, colon_pos));
        std::string value = line.substr(colon_pos + 1);
        while (!value.empty() && value.front() == ' ') {
            value.erase(value.begin());
        }
        request.headers[name] = value;
    }

    return true;
}

std::string build_response(int status_code, const std::string &status_text,
                           const std::string &body, const std::string &content_type) {
    std::ostringstream response;
    response << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";
    response << "Content-Length: " << body.size() << "\r\n";
    response << "Content-Type: " << content_type << "\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";
    response << body;
    return response.str();
}

std::string sanitize_target(std::string target) {
    auto query_pos = target.find('?');
    if (query_pos != std::string::npos) {
        target = target.substr(0, query_pos);
    }
    if (target.empty() || target[0] != '/') {
        target = "/";
    }
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

int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void handle_client_request(int client_fd, ClientState &state, const std::filesystem::path &root) {
    HttpRequest request;
    if (!parse_request(state.request_buffer, request)) {
        auto response = build_response(400, "Bad Request", "Bad Request", "text/plain; charset=utf-8");
        state.response_queue.push_back(response);
        return;
    }

    if (request.method != "GET") {
        auto response = build_response(405, "Method Not Allowed", "Method Not Allowed", "text/plain; charset=utf-8");
        state.response_queue.push_back(response);
        return;
    }

    if (request.version != "HTTP/1.1") {
        auto response = build_response(505, "HTTP Version Not Supported", "HTTP Version Not Supported", "text/plain; charset=utf-8");
        state.response_queue.push_back(response);
        return;
    }

    std::string target = sanitize_target(request.target);
    if (target == "/") {
        target = "/index.html";
    }

    if (target.find("..") != std::string::npos) {
        auto response = build_response(400, "Bad Request", "Bad Request", "text/plain; charset=utf-8");
        state.response_queue.push_back(response);
        return;
    }

    std::filesystem::path file_path = root / target.substr(1);

    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        auto response = build_response(404, "Not Found", "Not Found", "text/plain; charset=utf-8");
        state.response_queue.push_back(response);
        return;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string body = buffer.str();
    std::string content_type = get_content_type(file_path);

    auto response = build_response(200, "OK", body, content_type);
    state.response_queue.push_back(response);
}

}  // namespace

int main(int argc, char *argv[]) {
    int port = kDefaultPort;
    std::filesystem::path root = "www";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind("--port=", 0) == 0) {
            try {
                port = std::stoi(arg.substr(7));
            } catch (...) {
            }
        } else if (arg.rfind("--root=", 0) == 0) {
            root = arg.substr(7);
        }
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "socket failed: " << std::strerror(errno) << "\n";
        return 1;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "setsockopt failed: " << std::strerror(errno) << "\n";
        close(server_fd);
        return 1;
    }

    if (set_nonblocking(server_fd) < 0) {
        std::cerr << "fcntl failed: " << std::strerror(errno) << "\n";
        close(server_fd);
        return 1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(server_fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0) {
        std::cerr << "bind failed: " << std::strerror(errno) << "\n";
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, SOMAXCONN) < 0) {
        std::cerr << "listen failed: " << std::strerror(errno) << "\n";
        close(server_fd);
        return 1;
    }

    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        std::cerr << "epoll_create1 failed: " << std::strerror(errno) << "\n";
        close(server_fd);
        return 1;
    }

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) < 0) {
        std::cerr << "epoll_ctl failed: " << std::strerror(errno) << "\n";
        close(epoll_fd);
        close(server_fd);
        return 1;
    }

    std::map<int, ClientState> clients;
    std::vector<epoll_event> events(kMaxEvents);

    std::cout << "Non-blocking HTTP server listening on port " << port << " (root: " << root << ")\n";

    while (true) {
        int nfds = epoll_wait(epoll_fd, events.data(), kMaxEvents, -1);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            std::cerr << "epoll_wait failed: " << std::strerror(errno) << "\n";
            break;
        }

        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;

            if (fd == server_fd) {
                // Accept new connections
                while (true) {
                    sockaddr_in client_addr{};
                    socklen_t client_len = sizeof(client_addr);
                    int client_fd = accept(server_fd, reinterpret_cast<sockaddr *>(&client_addr), &client_len);
                    if (client_fd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;
                        }
                        std::cerr << "accept failed: " << std::strerror(errno) << "\n";
                        break;
                    }

                    if (set_nonblocking(client_fd) < 0) {
                        std::cerr << "fcntl failed: " << std::strerror(errno) << "\n";
                        close(client_fd);
                        continue;
                    }

                    clients[client_fd] = ClientState{};

                    epoll_event client_ev{};
                    client_ev.events = EPOLLIN;
                    client_ev.data.fd = client_fd;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_ev) < 0) {
                        std::cerr << "epoll_ctl failed: " << std::strerror(errno) << "\n";
                        close(client_fd);
                        clients.erase(client_fd);
                    }
                }
            } else {
                // Handle client I/O
                auto it = clients.find(fd);
                if (it == clients.end()) continue;

                ClientState &state = it->second;

                if (events[i].events & EPOLLIN && !state.request_complete) {
                    std::vector<char> buffer(kBufferSize);
                    while (true) {
                        ssize_t bytes = recv(fd, buffer.data(), buffer.size(), 0);
                        if (bytes < 0) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                break;
                            }
                            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
                            close(fd);
                            clients.erase(it);
                            break;
                        }
                        if (bytes == 0) {
                            // Connection closed
                            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
                            close(fd);
                            clients.erase(it);
                            break;
                        }
                        state.request_buffer.append(buffer.data(), static_cast<size_t>(bytes));
                        if (state.request_buffer.size() > kMaxRequestSize) {
                            auto resp = build_response(413, "Payload Too Large", "Payload Too Large", "text/plain; charset=utf-8");
                            state.response_queue.push_back(resp);
                            state.request_complete = true;
                            break;
                        }
                    }

                    if (it != clients.end() && state.is_request_complete()) {
                        handle_client_request(fd, state, root);
                        state.request_complete = true;

                        epoll_event client_ev{};
                        client_ev.events = EPOLLOUT;
                        client_ev.data.fd = fd;
                        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &client_ev);
                    }
                }

                if (events[i].events & EPOLLOUT && state.request_complete && !state.response_queue.empty()) {
                    auto &response = state.response_queue.front();
                    while (state.response_sent < response.size()) {
                        ssize_t sent = send(fd, response.data() + state.response_sent, response.size() - state.response_sent, 0);
                        if (sent < 0) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                break;
                            }
                            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
                            close(fd);
                            clients.erase(it);
                            break;
                        }
                        state.response_sent += static_cast<size_t>(sent);
                    }

                    if (it != clients.end() && state.response_sent >= response.size()) {
                        state.response_queue.pop_front();
                        if (state.response_queue.empty()) {
                            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
                            close(fd);
                            clients.erase(it);
                        } else {
                            state.response_sent = 0;
                        }
                    }
                }
            }
        }
    }

    close(epoll_fd);
    close(server_fd);
    return 0;
}
