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

#include "resp_parser.h"
#include "utils.h"
#include "storage.h"

#define debug 0

using RESP::Token;

void handleCmd(const std::string &input, const int client_fd) {
#if debug
    printRaw(input);
#endif

    auto token = RESP::parse(input);
    const auto &cmd = (token.getDataType() == Token::DataType::ARRAY ?  token.getArray()[0].getString() : token.getString());

    if (cmd == "echo") {
        const auto &args = token.getArray();
        const auto &s = args[1].getString();
        const std::string response = "$" + std::to_string(s.size()) + "\r\n" + s + "\r\n";
        send(client_fd, response.c_str(), strlen(response.c_str()), 0);
    } else if (cmd == "ping") {
        const auto response = "+PONG\r\n";
        send(client_fd, response, strlen(response), 0);
    } else if (cmd == "set") {
        const auto &args = token.getArray();

        if (args.size() == 5) {
            const float expirationTime = (args[3].getString() == "ex" ? 1000.0f : 1.0f) * static_cast<float>(std::stoi(args[4].getString()));
            storage.set(args[1].getString(), args[2].getString(), true, expirationTime);
        } else if (args.size() == 3) {
            storage.set(args[1].getString(), args[2].getString());
        }

        send(client_fd, Responses::OK, strlen(Responses::OK), 0);
    } else if (cmd == "get") {
        const auto &args = token.getArray();
        if (const auto value = storage.get(args[1].getString())) {
            const std::string response = RESP::encodeIntoBulkString(*value);
            send(client_fd, response.c_str(), response.size(), 0);
        } else send(client_fd, Responses::NullBulkString, strlen(Responses::NullBulkString), 0);
    } else if (cmd == "rpush" || cmd == "lpush") {
        const auto &args = token.getArray();
        const auto &key = args[1].getString();
        for (int i = 2; i < args.size(); i++) {
            storage.addToArray(key, args[i].getString(), cmd == "lpush");
        }
        const std::string response = RESP::encodeIntoInt(storage.sizeOfArray(key));
        send(client_fd, response.c_str(), response.size(), 0);
    } else if (cmd == "lrange") {
        const auto &args = token.getArray();
        const auto arr = storage.getArray(
            args[1].getString(),
            std::stoi(args[2].getString()),
            std::stoi(args[3].getString())
        );
        const std::string response = RESP::encodeIntoArray(arr);
        send(client_fd, response.c_str(), response.size(), 0);
    }
}

void handle_client(const int client_fd) {
    std::string inputBuffer;

    char buffer[1024];
    while (true) {
        auto bytes_received = recv(client_fd, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            // std::cerr << "Client Disconnected\n";
            break;
        }

        inputBuffer.append(buffer, bytes_received);

#if debug
        printRaw(inputBuffer);
#endif

        handleCmd(inputBuffer, client_fd);
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

    // Testing String
    // auto token = RESP::parse("*4\r\n:10001\r\n+Hello Bye!\r\n$12\r\nMaybe Maybe!\r\n*2\r\n+Test 1\r\n+Test 2\r\n");

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
    server_addr.sin_port = htons(6379);

    if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
        std::cerr << "Failed to bind to port 6379\n";
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
