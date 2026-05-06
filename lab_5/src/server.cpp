#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <process.h>
#include "../include/cache.h"

#define PORT 8080
#define STATIC_PATH "public"
#define BUFFER_SIZE 4096

typedef SOCKET socket_t;
#define CLOSESOCKET closesocket

static void log_socket_error(const char *message) {
    std::fprintf(stderr, "%s (err=%d)\n", message, WSAGetLastError());
}

unsigned __stdcall handle_client(void *client_socket_ptr);
static void send_response(socket_t client_socket, const char *status, const char *content_type, const char *body, int body_len);
static void send_file_response(socket_t client_socket, const char *filepath);
static void send_404(socket_t client_socket);

static int is_invalid_socket(socket_t socket_fd) {
    return socket_fd == INVALID_SOCKET;
}

#ifdef _WIN32
int main(int argc, char** argv);
int WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return main(__argc, __argv);
}
#endif

int main(int, char**) {
    socket_t server_socket, client_socket;
    sockaddr_in server_addr, client_addr;
    int client_addr_len = sizeof(client_addr);
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        log_socket_error("WSAStartup failed");
        return 1;
    }
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (is_invalid_socket(server_socket)) {
        log_socket_error("Socket creation failed");
        return 1;
    }
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&opt), sizeof(opt)) < 0) {
        log_socket_error("Setsockopt failed");
        CLOSESOCKET(server_socket);
        return 1;
    }
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    if (bind(server_socket, reinterpret_cast<sockaddr *>(&server_addr), sizeof(server_addr)) < 0) {
        log_socket_error("Bind failed");
        CLOSESOCKET(server_socket);
        return 1;
    }
    if (listen(server_socket, 10) < 0) {
        log_socket_error("Listen failed");
        CLOSESOCKET(server_socket);
        return 1;
    }
    std::printf("Server listening on port %d...\n", PORT);
    while (true) {
        client_socket = accept(server_socket, reinterpret_cast<sockaddr *>(&client_addr), &client_addr_len);
        if (is_invalid_socket(client_socket)) {
            log_socket_error("Accept failed");
            continue;
        }
        auto *new_sock = new (std::nothrow) socket_t(client_socket);
        if (!new_sock) {
            CLOSESOCKET(client_socket);
            continue;
        }
        uintptr_t thread = _beginthreadex(NULL, 0, handle_client, static_cast<void *>(new_sock), 0, NULL);
        if (thread == 0) {
            log_socket_error("Thread creation failed");
            CLOSESOCKET(client_socket);
            delete new_sock;
        } else {
            CloseHandle(reinterpret_cast<HANDLE>(thread));
        }
    }
    CLOSESOCKET(server_socket);
    WSACleanup();
    return 0;
}

unsigned __stdcall handle_client(void *client_socket_ptr) {
    std::unique_ptr<socket_t> client_socket_owner(static_cast<socket_t *>(client_socket_ptr));
    socket_t client_socket = *client_socket_owner;
    char buffer[BUFFER_SIZE];
    int bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        char method[16], path[256], protocol[16];
        if (std::sscanf(buffer, "%15s %255s %15s", method, path, protocol) == 3) {
            if (std::strcmp(method, "GET") == 0) {
                if (std::strcmp(path, "/") == 0) {
                    std::strcpy(path, "/index.html");
                }
                const std::string filepath = std::string(STATIC_PATH) + path;
                if (filepath.find("..") != std::string::npos) {
                    send_404(client_socket);
                } else {
                    send_file_response(client_socket, filepath.c_str());
                }
            } else {
                const char *response = "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\n\r\n";
                send(client_socket, response, static_cast<int>(std::strlen(response)), 0);
            }
        }
    }
    CLOSESOCKET(client_socket);
    return 0;
}

static void send_file_response(socket_t client_socket, const char *filepath) {
    FileCache *file_info = get_file_cache(filepath);
    if (file_info == NULL) {
        send_404(client_socket);
        return;
    }
    char header[512];
    int header_len = std::snprintf(header, sizeof(header),
                                   "HTTP/1.1 200 OK\r\n"
                                   "Content-Type: %s\r\n"
                                   "Content-Length: %zu\r\n"
                                   "Connection: close\r\n"
                                   "\r\n",
                                   file_info->content_type.c_str(), file_info->file_size);
    send(client_socket, header, header_len, 0);
    LARGE_INTEGER zero;
    zero.QuadPart = 0;
    SetFilePointerEx(file_info->handle, zero, NULL, FILE_BEGIN);
    char buffer[BUFFER_SIZE];
    std::size_t remaining = file_info->file_size;
    while (remaining > 0) {
        DWORD to_read = (remaining > BUFFER_SIZE) ? BUFFER_SIZE : static_cast<DWORD>(remaining);
        DWORD bytes_read = 0;
        if (!ReadFile(file_info->handle, buffer, to_read, &bytes_read, NULL) || bytes_read == 0) {
            break;
        }
        if (send(client_socket, buffer, static_cast<int>(bytes_read), 0) == SOCKET_ERROR) {
            break;
        }
        remaining -= bytes_read;
    }
}

static void send_404(socket_t client_socket) {
    const char *body = "<html><body><h1>404 Not Found</h1></body></html>";
    send_response(client_socket, "404 Not Found", "text/html", body, static_cast<int>(std::strlen(body)));
}

static void send_response(socket_t client_socket, const char *status, const char *content_type, const char *body, int body_len) {
    char header[512];
    int header_len = std::snprintf(header, sizeof(header),
                                   "HTTP/1.1 %s\r\n"
                                   "Content-Type: %s\r\n"
                                   "Content-Length: %d\r\n"
                                   "Connection: close\r\n"
                                   "\r\n",
                                   status, content_type, body_len);
    send(client_socket, header, header_len, 0);
    send(client_socket, body, body_len, 0);
}
