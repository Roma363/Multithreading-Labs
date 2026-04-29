#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr int kDefaultPort = 8080;
constexpr size_t kMaxRequestSize = 8192;

std::string to_lower(std::string value) {
    for (char &ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

bool send_all(int client_fd, const std::string &data) {
    size_t total_sent = 0;
    while (total_sent < data.size()) {
        ssize_t sent = send(client_fd, data.data() + total_sent, data.size() - total_sent, 0);
        if (sent <= 0) {
            return false;
        }
        total_sent += static_cast<size_t>(sent);
    }
    return true;
}

std::string get_content_type(const std::filesystem::path &path) {
    auto ext = to_lower(path.extension().string());
    if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
    if (ext == ".txt") return "text/plain; charset=utf-8";
    if (ext == ".css") return "text/css; charset=utf-8";
    if (ext == ".js") return "application/javascript; charset=utf-8";
    return "application/octet-stream";
}

bool read_request(int client_fd, std::string &out) {
    out.clear();
    std::vector<char> buffer(1024);
    while (out.size() < kMaxRequestSize) {
        ssize_t bytes = recv(client_fd, buffer.data(), buffer.size(), 0);
        if (bytes < 0) {
            return false;
        }
        if (bytes == 0) {
            break;
        }
        out.append(buffer.data(), static_cast<size_t>(bytes));
        if (out.find("\r\n\r\n") != std::string::npos) {
            return true;
        }
    }
    return !out.empty();
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

void handle_client(int client_fd, const std::filesystem::path &root) {
    std::string raw_request;
    if (!read_request(client_fd, raw_request)) {
        close(client_fd);
        return;
    }

    HttpRequest request;
    if (!parse_request(raw_request, request)) {
        std::string body = "Bad Request";
        auto response = build_response(400, "Bad Request", body, "text/plain; charset=utf-8");
        send_all(client_fd, response);
        close(client_fd);
        return;
    }

    if (request.method != "GET") {
        std::string body = "Method Not Allowed";
        auto response = build_response(405, "Method Not Allowed", body, "text/plain; charset=utf-8");
        send_all(client_fd, response);
        close(client_fd);
        return;
    }

    if (request.version != "HTTP/1.1") {
        std::string body = "HTTP Version Not Supported";
        auto response = build_response(505, "HTTP Version Not Supported", body, "text/plain; charset=utf-8");
        send_all(client_fd, response);
        close(client_fd);
        return;
    }

    std::string target = sanitize_target(request.target);
    if (target == "/") {
        target = "/index.html";
    }

    if (target.find("..") != std::string::npos) {
        std::string body = "Bad Request";
        auto response = build_response(400, "Bad Request", body, "text/plain; charset=utf-8");
        send_all(client_fd, response);
        close(client_fd);
        return;
    }

    std::filesystem::path file_path = root / target.substr(1);

    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        std::string body = "Not Found";
        auto response = build_response(404, "Not Found", body, "text/plain; charset=utf-8");
        send_all(client_fd, response);
        close(client_fd);
        return;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string body = buffer.str();
    std::string content_type = get_content_type(file_path);

    auto response = build_response(200, "OK", body, content_type);
    send_all(client_fd, response);
    close(client_fd);
}

int parse_port(const std::string &value, int fallback) {
    try {
        int port = std::stoi(value);
        if (port > 0 && port <= 65535) {
            return port;
        }
    } catch (...) {
    }
    return fallback;
}

}  // namespace

int main(int argc, char *argv[]) {
    int port = kDefaultPort;
    std::filesystem::path root = "www";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind("--port=", 0) == 0) {
            port = parse_port(arg.substr(7), port);
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

    std::cout << "HTTP server listening on port " << port << " (root: " << root << ")\n";

    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, reinterpret_cast<sockaddr *>(&client_addr), &client_len);
        if (client_fd < 0) {
            std::cerr << "accept failed: " << std::strerror(errno) << "\n";
            continue;
        }

        std::thread([client_fd, root]() { handle_client(client_fd, root); }).detach();
    }

    close(server_fd);
    return 0;
}
