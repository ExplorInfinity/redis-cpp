#include <algorithm>
#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "commands.h"
#include "resp_parser.h"
#include "utils.h"
#include "storage.h"

#define debug 0

void handle_client(const int client_fd) {
    std::string inputBuffer;

    char buffer[1024];
    while (true) {
        auto bytes_received = recv(client_fd, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
#if debug
        std::cerr << "Client Disconnected\n";
#endif
            break;
        }

        inputBuffer.append(buffer, bytes_received);

#if debug
        printRaw(inputBuffer);
#endif

        Commands::handleCmd(client_fd, inputBuffer);
        inputBuffer.clear();
    }

#if debug
    std::cout << "Client Disconnected\n";
#endif

    close(client_fd);
}

int main(int argc, char **argv) {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    int PORT = 6379;
    for (int i = 0; i < argc; i++) {
        if (std::string(argv[i]) == "--port" && i + 1 < argc && isNumericValue(argv[i + 1]))
            PORT = std::stoi(argv[i + 1]);
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Failed to create server socket\n";
        return 1;
    }

    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        std::cerr << "setsockopt failed\n";
        return 1;
    }

    struct sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
        std::cerr << std::format("Failed to bind to port {}\n", PORT);
        return 1;
    }

    int connection_backlog = 5;
    if (listen(server_fd, connection_backlog) != 0) {
        std::cerr << "listen failed\n";
        return 1;
    }

    struct sockaddr_in client_addr{};
    int client_addr_len = sizeof(client_addr);
    std::cout << "Waiting for a client to connect...\n";

    while (true) {
        int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
        std::cout << "Client connected\n";

        std::thread(handle_client, client_fd).detach();
    }

    close(server_fd);
    return 0;
}