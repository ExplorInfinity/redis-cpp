#pragma once

#include <string>
#include <mutex>
#include <condition_variable>

using ll = long long;

extern std::mutex replicaMutex;
extern std::condition_variable cvReplica;

struct Replica {
private:
    inline static int tracker_id = 0;

public:
    int id;
    int server_fd{0};
    ll offset{0};

    Replica() {
        id = tracker_id++;
    };

    explicit Replica(const int server_fd)
        : server_fd(server_fd)
    {
        id = tracker_id++;
    }

    static void initializeHandshake(const std::string &IP, int PORT);
};