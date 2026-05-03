#pragma once
#include <vector>
#include <string>
#include <unordered_map>

using StringMap = std::unordered_map<std::string, std::string>;

enum class ServerStatus { UNREPLICATED, REPLICATED };

struct Worker {
    ServerStatus status = ServerStatus::UNREPLICATED;
    int server_fd{-1};

    Worker() = default;

    explicit Worker(const int server_fd)
        : server_fd(server_fd) { }

    static void initializeHandshake(const std::string &IP, int PORT);
};


extern std::vector<Worker> workers;
std::string getRDB();

extern StringMap ServerInfo;
void setServerInfo(int argc, char **argv);

extern bool isReplica;
extern int replicaOffset;