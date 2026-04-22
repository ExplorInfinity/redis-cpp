#include "utils.h"

#include <iostream>

void printRaw(const std::string &s) {
    for (const char ch : s) {
        switch (ch) {
            case '\r':
                std::cout << "\\r";
                break;
            case '\n':
                std::cout << "\\n";
                break;
            default:
                std::cout << ch;
        }
    }

    std::cout << std::endl;
}