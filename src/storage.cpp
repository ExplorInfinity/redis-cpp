#include "storage.h"

#include <iostream>
#include <string>
#include <unordered_map>
#include <optional>

#define debug 0

void Storage::set(const std::string &key, const std::string &value) {
    map[key] = value;
}

std::optional<std::string> Storage::get(const std::string &key) {
#if debug
    std::cout << "Get Request: " << key << std::endl;
#endif

    if (!map.contains(key))
        return std::nullopt;

    return map.at(key);
}