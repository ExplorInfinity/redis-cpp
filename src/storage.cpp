#include "storage.h"

#include <iostream>
#include <string>
#include <unordered_map>
#include <optional>

#define debug 0

void Storage::set(const std::string &key, const std::string &value) {
    map.insert_or_assign(key, StorageValue(value));
}

void Storage::set(const std::string &key, const std::string &value, const bool expires, const float expirationTime) {
#if debug
    std::cout << "Expiration Time: " << expirationTime << std::endl;
#endif

    map.insert_or_assign(key, StorageValue(value, expires, expirationTime));
}

std::optional<std::string> Storage::get(const std::string &key) {
#if debug
    std::cout << "Get Request: " << key << std::endl;
#endif

    if (!map.contains(key) || map.at(key).expired())
        return std::nullopt;

    return map.at(key).value;
}