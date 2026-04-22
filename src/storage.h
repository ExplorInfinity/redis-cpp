#pragma once

#include <string>
#include <unordered_map>
#include <optional>
#include <chrono>
#include <utility>

class Timer {
    std::chrono::time_point<std::chrono::high_resolution_clock> start;

public:
    Timer() {
        start = std::chrono::high_resolution_clock::now();
    }

    [[nodiscard]] float getElapsedTime() const {
        const std::chrono::time_point<std::chrono::high_resolution_clock> end = std::chrono::high_resolution_clock::now();
        const std::chrono::duration<float> duration = end - start;
        const float ms = duration.count() * 1000.0f;
        return ms;
    }
};

struct StorageValue {

    std::string value;

private:
    bool expires = false;
    float expirationTime = -1;
    Timer timer;

public:
    explicit StorageValue(std::string value)
        : value(std::move(value)) { }

    explicit StorageValue(std::string value, const bool expires, const float expirationTime)
        : value(std::move(value)), expires(expires), expirationTime(expirationTime), timer() { }

    [[nodiscard]] bool expired() const {
        return (expires ? timer.getElapsedTime() > expirationTime : false);
    }
};

class Storage {
    std::unordered_map<std::string, StorageValue> map;

public:
    void set(const std::string &key, const std::string &value);
    void set(const std::string &key, const std::string &value, bool expires, float expirationTime);

    std::optional<std::string> get(const std::string &key);
};

inline Storage storage;