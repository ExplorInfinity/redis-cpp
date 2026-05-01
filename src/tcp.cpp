#include "tcp.h"

#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "resp_parser.h"

#define CONNECTION_FAILED (-1)

int TCP::connectToServer(std::string IP, int PORT) {
    if (IP == "localhost")
        IP = "127.0.0.1";

    const int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Socket creation failed" << std::endl;
        return CONNECTION_FAILED;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, IP.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "Invalid IP Address" << std::endl;
        return CONNECTION_FAILED;
    }

    if (connect(sock, (sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Connection Failed" << std::endl;
        return CONNECTION_FAILED;
    }

    std::cout << "Connected to server with IP " << IP << " on PORT " << PORT << std::endl;

    return sock;
}

void TCP::closeConnection(const int sock) {
    close(sock);
}
