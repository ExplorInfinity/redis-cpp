#include "tcp.h"

#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

void TCP::connectToServer(std::string IP, int PORT) {
    if (IP == "localhost")
        IP = "127.0.0.1";

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Socket creation failed" << std::endl;
        return;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, IP.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "Invalid IP Address" << std::endl;
        return;
    }

    if (connect(sock, (sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Connection Failed" << std::endl;
        return;
    }

    std::cout << "Connected to server with IP " << IP << " on PORT " << PORT << std::endl;

    const char* msg = "*1\r\n$4\r\nPING\r\n";
    send(sock, msg, strlen(msg), 0);

    char buffer[1024]{};
    const auto bytesReceived = read(sock, buffer, sizeof(buffer));

    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';
        std::cout << "Server Response " << buffer << std::endl;
    }

    close(sock);
}
