#pragma once

#include <string>
#include <unordered_map>
#include <optional>
#include <utility>
#include <vector>

#include "timer.h"

class Value {
protected:
    bool expires = false;
    float expirationTime = -1;
    Timer timer;

public:
    Value() = default;

    explicit Value(const bool expires, const float expirationTime)
        : expires(expires), expirationTime(expirationTime) { }

    [[nodiscard]] bool expired() const {
        return (expires ? timer.getElapsedTime() > expirationTime : false);
    }
};

class StringValue : public Value {
public:
    std::string value;

    explicit StringValue(std::string value)
        : value(std::move(value)) { }

    explicit StringValue(std::string value, const bool expires, const float expirationTime)
        : Value(expires, expirationTime), value(std::move(value)) { }
};

class ArrayValue : public Value {
public:
    std::vector<std::string> values;

    ArrayValue() = default;

    explicit ArrayValue(std::string value) {
        values.push_back(std::move(value));
    }

    explicit ArrayValue(std::string value, const bool expires, const float expirationTime)
        : Value(expires, expirationTime)
    {
        values.push_back(std::move(value));
    }
};


class Storage {
    std::unordered_map<std::string, StringValue> str_storage;
    std::unordered_map<std::string, ArrayValue> arr_storage;

public:
    void set(const std::string &key, const std::string &value);
    void set(const std::string &key, const std::string &value, bool expires, float expirationTime);

    void addToArray(const std::string &key, const std::string &value);
    [[nodiscard]] std::size_t sizeOfArray(const std::string &key) const;

    std::optional<std::string> get(const std::string &key);
};

inline Storage storage;