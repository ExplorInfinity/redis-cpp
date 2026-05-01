#include "tcp.h"

#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "resp_parser.h"

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

    char buffer[1024]{};

    const std::string PING = RESP::encodeIntoArray({ "PING" });
    send(sock, PING.c_str(), PING.size(), 0);
    read(sock, buffer, sizeof(buffer));

    const std::string REPLCONF = RESP::encodeIntoArray({ "REPLCONF", "listening-port", "6380" });
    send(sock, REPLCONF.c_str(), REPLCONF.size(), 0);
    read(sock, buffer, sizeof(buffer));

    const std::string PSYNC = RESP::encodeIntoArray({ "REPLCONF", "capa", "psync2" });
    send(sock, PSYNC.c_str(), PSYNC.size(), 0);
    const auto bytesReceived = read(sock, buffer, sizeof(buffer));

    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';
        std::cout << "Server Response " << buffer << std::endl;
    }

    close(sock);
}
