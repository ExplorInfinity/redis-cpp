#include "utils.h"

#include <ranges>
#include <algorithm>
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

bool isNumericValue(const std::string &s) {
    try {
        size_t pos;
        std::stoll(s, &pos);
        return pos == s.size();
    } catch (...) {
        return false;
    }
}

std::string convertToUpperCase(std::string s) {
    std::ranges::transform(s, s.begin(), [] (unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return s;
}

std::string convertToLowerCase(std::string s) {
    std::ranges::transform(s, s.begin(), [] (unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

bool isDouble(const std::string &s) {
    try {
        size_t pos;
        std::stod(s, &pos);
        return pos == s.size();
    } catch (...) {
        return false;
    }
}

bool isFlag(const std::string &s) {
    return (s.size() > 2 && s.substr(0, 2) == "--");
}

StringMap parseProgramArgs(int argc, char **argv) {
    StringMap args;

    for (int i = 0; i < argc; i++) {
        const std::string curr = argv[i];
        if (isFlag(curr)) {
            if (i + 1 < argc && !isFlag(std::string(argv[i + 1]))) {
                args[curr] = argv[i + 1];
                i++;
            } else args[curr] = "true";
        }
    }

    return args;
}
