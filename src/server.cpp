#include "server.h"

#include <iostream>
#include <sstream>
#include <ranges>
#include <arpa/inet.h>

#include "utils.h"
#include "commands.h"

int Server::port = 6379;
ll Server::replOffset = 0;
ll Server::master_replOffset = 0;
bool Server::isReplica = false;
std::string Server::replId = "8371b4fb1155b71f4a04d3e1bc3e18c4a990aeeb";
StringMap Server::info;
ReplicaMap Server::replicas;

std::string Server::getRDB() {
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

void Server::setServerInfo(const int argc, char **argv) {
    info = parseProgramArgs(argc, argv);

    if (const auto it = info.find("--port"); it != info.end())
        port = std::stoi(it->second);

    if (const auto it = info.find("--replicaof"); it != info.end()) {
        isReplica = true;
        std::stringstream ss(it->second);

        std::string master_ip;
        if (int master_port; ss >> master_ip >> master_port)
            Replica::initializeHandshake(master_ip, master_port);
        else
            std::cerr << "Invalid --replicaof format. Expected: <ip> <port>" << std::endl;
    }
}

void Server::addReplica(const int replica_fd) {
    replicas[replica_fd].server_fd = replica_fd;
}

void Server::propagateToReplicas(const std::string &s) {
    for (const auto &replica_fd: replicas | std::views::keys)
        send(replica_fd, s.c_str(), s.size(), 0);
    master_replOffset += static_cast<int>(s.size());
}

int Server::countReplicasWithTargetOffset(const ll targetOffset) {
    int count = 0;
    for (const auto &replica: replicas | std::views::values) {
        count += replica.offset >= targetOffset;
    }
    return count;
}
