#include "replica.h"

#include <unistd.h>
#include <arpa/inet.h>
#include <thread>

#include "tcp.h"
#include "resp_parser.h"
#include "commands.h"

std::mutex replicaMutex;
std::condition_variable cvReplica;

void Replica::initializeHandshake(const std::string &IP, const int PORT) {
    const int master_fd = TCP::connectToServer(IP, PORT);

    char buffer[4096];

    const std::string PING = RESP::encodeIntoArray({ "PING" });
    send(master_fd, PING.c_str(), PING.size(), 0);
    read(master_fd, buffer, sizeof(buffer));

    const std::string REPLCONF = RESP::encodeIntoArray({ "REPLCONF", "listening-port", "6380" });
    send(master_fd, REPLCONF.c_str(), REPLCONF.size(), 0);
    read(master_fd, buffer, sizeof(buffer));

    const std::string REPLCONF2 = RESP::encodeIntoArray({ "REPLCONF", "capa", "psync2" });
    send(master_fd, REPLCONF2.c_str(), REPLCONF2.size(), 0);
    read(master_fd, buffer, sizeof(buffer));

    const std::string PSYNC = RESP::encodeIntoArray({ "PSYNC", "?", "-1" });
    send(master_fd, PSYNC.c_str(), PSYNC.size(), 0);
    auto receivedBytes = read(master_fd, buffer, sizeof(buffer));

    std::string inputBuffer;
    inputBuffer.append(buffer, receivedBytes);

    int cursor = 0;
    RESP::parseSimpleString(inputBuffer, cursor);
    inputBuffer = inputBuffer.substr(cursor);

    while (inputBuffer.find(RESP::CRLF) == std::string::npos) {
        receivedBytes = read(master_fd, buffer, sizeof(buffer));
        inputBuffer.append(buffer, receivedBytes);
    }

    auto endSizeIndex = inputBuffer.find(RESP::CRLF);
    int totalBytes = std::stoi(inputBuffer.substr(1, endSizeIndex));
    inputBuffer = inputBuffer.substr(endSizeIndex + RESP::CRLF.size());

    while (inputBuffer.size() < totalBytes) {
        receivedBytes = read(master_fd, buffer, sizeof(buffer));
        inputBuffer.append(buffer, receivedBytes);
    }

    inputBuffer = inputBuffer.erase(0, totalBytes);

    std::thread([master_fd, inputBuffer = std::move(inputBuffer)] mutable  {
        char buffer[4096];
        while (true) {
            const int bytesUsed = Commands::handleCmd(master_fd, inputBuffer, false);
            inputBuffer.erase(0, bytesUsed);

            const auto bytes_received = recv(master_fd, buffer, sizeof(buffer), 0);
            if (bytes_received <= 0)
                break;
            inputBuffer.append(buffer, bytes_received);
        }

        TCP::closeConnection(master_fd);
    }).detach();
}