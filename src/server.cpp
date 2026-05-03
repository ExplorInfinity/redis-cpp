#include "server.h"

#include <iostream>
#include <thread>
#include <arpa/inet.h>
#include <unistd.h>

#include "commands.h"
#include "tcp.h"
#include "resp_parser.h"

void Worker::initializeHandshake(const std::string &IP, const int PORT) {
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

    int endSizeIndex = inputBuffer.find(RESP::CRLF);
    int totalBytes = std::stoi(inputBuffer.substr(1, endSizeIndex));
    inputBuffer = inputBuffer.substr(endSizeIndex + RESP::CRLF.size());

    while (inputBuffer.size() < totalBytes) {
        receivedBytes = read(master_fd, buffer, sizeof(buffer));
        inputBuffer.append(buffer, receivedBytes);
    }

    inputBuffer = inputBuffer.substr(totalBytes);

    std::thread([master_fd, inputBuffer = std::move(inputBuffer)] mutable  {
        char buffer[4096];
        while (true) {
            const auto bytes_received = recv(master_fd, buffer, sizeof(buffer), 0);
            if (bytes_received <= 0)
                break;

            inputBuffer.append(buffer, bytes_received);
            Commands::handleCmd(master_fd, inputBuffer, false);
            replicaOffset += static_cast<int>(bytes_received);
            inputBuffer.clear();
        }

        TCP::closeConnection(master_fd);
    }).detach();
}

std::vector<Worker> workers;

std::string getRDB() {
    const unsigned char emptyRDB[] = {
        0x52, 0x45, 0x44, 0x49, 0x53, 0x30, 0x30, 0x31, 0x31, 0xfa, 0x09, 0x72, 0x65, 0x64, 0x69, 0x73,
        0x2d, 0x76, 0x65, 0x72, 0x05, 0x37, 0x2e, 0x32, 0x2e, 0x30, 0xfa, 0x0a, 0x72, 0x65, 0x64, 0x69,
        0x73, 0x2d, 0x62, 0x69, 0x74, 0x73, 0xc0, 0x40, 0xfa, 0x05, 0x63, 0x74, 0x69, 0x6d, 0x65, 0xc2,
        0x6d, 0x08, 0xbc, 0x65, 0xfa, 0x08, 0x75, 0x73, 0x65, 0x64, 0x2d, 0x6d, 0x65, 0x6d, 0xc2, 0xb0,
        0xc4, 0x10, 0x00, 0xfa, 0x08, 0x61, 0x6f, 0x66, 0x2d, 0x62, 0x61, 0x73, 0x65, 0xc0, 0x00, 0xff,
        0xf0, 0x6e, 0x3b, 0xfe, 0xc0, 0xff, 0x5a, 0xa2
    };

    std::string response = "$" + std::to_string(sizeof(emptyRDB)) + "\r\n";
    response += std::string(reinterpret_cast<const char *>(emptyRDB), sizeof(emptyRDB));
    return response;
}

StringMap ServerInfo;
void setServerInfo(const int argc, char **argv) {
    ServerInfo = parseProgramArgs(argc, argv);
    isReplica = ServerInfo.contains("--replicaof");
}

bool isReplica = false;
int replicaOffset = 0;