#pragma once

#include <string>
#include <unordered_map>

#include "replica.h"

using ll = long long;
using StringMap = std::unordered_map<std::string, std::string>;
using ReplicaMap = std::unordered_map<int, Replica>;

class Server {
public:
    static int port;
    static ll replOffset;
    static ll master_replOffset;
    static bool isReplica;
    static std::string replId;
    static StringMap info;
    static ReplicaMap replicas;

    Server() = delete;

    static std::string getRDB();
    static void setServerInfo(int argc, char **argv);
    static void addReplica(int replica_fd);
    static void propagateToReplicas(const std::string &s);
    static int countReplicasWithTargetOffset(ll targetOffset);
};