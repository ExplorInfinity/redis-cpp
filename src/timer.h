#pragma once

#include <chrono>

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