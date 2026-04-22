#include "storage.h"

#include <iostream>
#include <string>
#include <unordered_map>
#include <optional>

#define debug 0

void Storage::set(const std::string &key, const std::string &value) {
    str_storage.insert_or_assign(key, StringValue(value));
}

void Storage::set(const std::string &key, const std::string &value, const bool expires, const float expirationTime) {
#if debug
    std::cout << "Expiration Time: " << expirationTime << std::endl;
#endif

    str_storage.insert_or_assign(key, StringValue(value, expires, expirationTime));
}

void Storage::addToArray(const std::string &key, const std::string &value) {
    arr_storage[key].values.push_back(value);
}

std::size_t Storage::sizeOfArray(const std::string &key) const {
    if (!arr_storage.contains(key))
        return 0;

    return arr_storage.at(key).values.size();
}

std::optional<std::string> Storage::get(const std::string &key) {
#if debug
    std::cout << "Get Request: " << key << std::endl;
#endif

    if (!str_storage.contains(key) || str_storage.at(key).expired())
        return std::nullopt;

    return str_storage.at(key).value;
}
