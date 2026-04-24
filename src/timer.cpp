#include "timer.h"

#include <iostream>
long long getCurrTimeInMilliSecs() {
    const auto time = std::chrono::system_clock::now();
    std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()).count() << std::endl;
    return std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()).count();
};