#pragma once

#include <string>
#include <unordered_map>
#include <optional>
#include <utility>
#include <vector>
#include <memory>

#include "timer.h"

enum class ValueType { NIL, String, Stream, List };
inline std::vector<std::string> ValueTypeStringMap = { "nil", "string", "stream", "list" };

class Value {
protected:
    bool expires = false;
    float expirationTime = -1;
    Timer timer;

public:
    virtual ~Value() = default;

    Value() = default;

    explicit Value(const bool expires, const float expirationTime)
        : expires(expires), expirationTime(expirationTime) { }

    [[nodiscard]] bool expired() const {
        return (expires ? timer.getElapsedTime() > expirationTime : false);
    }

    [[nodiscard]] virtual ValueType getType() const = 0;
};

class StringValue : public Value {
public:
    std::string value;

    explicit StringValue(std::string value)
        : value(std::move(value)) { }

    explicit StringValue(std::string value, const bool expires, const float expirationTime)
        : Value(expires, expirationTime), value(std::move(value)) { }

    [[nodiscard]] ValueType getType() const override {
        return ValueType::String;
    };
};

class ListValue : public Value {
public:
    std::vector<std::string> values;

    ListValue() = default;

    explicit ListValue(std::string value) {
        values.push_back(std::move(value));
    }

    explicit ListValue(std::string value, const bool expires, const float expirationTime)
        : Value(expires, expirationTime)
    {
        values.push_back(std::move(value));
    }

    [[nodiscard]] ValueType getType() const override {
        return ValueType::List;
    };
};

class StreamValue : public Value {
public:
    std::string id;
    std::unordered_map<std::string, std::string> kv_pairs;

    StreamValue() = default;

    explicit StreamValue(std::string id)
        : id(std::move(id)) { }

    explicit StreamValue(std::string id, const bool expires, const float expirationTime)
        : Value(expires, expirationTime), id(std::move(id)) { }

    [[nodiscard]] ValueType getType() const override {
        return ValueType::Stream;
    };
};


class Storage {
    std::unordered_map<std::string, std::unique_ptr<Value>> kvStorage;
public:
    /* SET functions */
    template <class ValueClass>
    std::reference_wrapper<ValueClass> set(const std::string &key, const std::string &value) {
        auto &ref = kvStorage.insert_or_assign(key, std::make_unique<ValueClass>(value)).first->second;
        return std::ref(*dynamic_cast<ValueClass*>(ref.get()));
    }

    template <class ValueClass>
    std::reference_wrapper<ValueClass> set(const std::string &key, const std::string &value, const bool expires, const float expirationTime) {
        auto &ref = kvStorage.insert_or_assign(key, std::make_unique<ValueClass>(value, expires, expirationTime)).first->second;
        return std::ref(*dynamic_cast<ValueClass*>(ref.get()));
    }

    /* GET functions */
    template <class ValueClass>
    [[nodiscard]] std::optional<std::reference_wrapper<ValueClass>> get(const std::string &key) {
        const auto it = kvStorage.find(key);

        if (it == kvStorage.end() || it->second->expired())
            return std::nullopt;

        if (auto *value = dynamic_cast<ValueClass*>(it->second.get()))
            return std::ref(*value);

        return std::nullopt;
    }

    template <class ValueClass>
    [[nodiscard]] std::optional<std::reference_wrapper<const ValueClass>> get(const std::string &key) const {
        const auto it = kvStorage.find(key);

        if (it == kvStorage.end() || it->second->expired())
            return std::nullopt;

        if (auto *value = dynamic_cast<const ValueClass*>(it->second.get()))
            return std::cref(*value);

        return std::nullopt;
    }

    /* List Helper functions */
    void appendToList(const std::string &key, const std::string &value, bool prepend = false);
    [[nodiscard]] std::vector<std::string> getListCopy(const std::string &key, int start, int stop) const;
    [[nodiscard]] std::size_t sizeOfList(const std::string &key) const;
    [[nodiscard]] bool containsList(const std::string &key) const;
    std::string RPOP(const std::string &key);
    std::string LPOP(const std::string &key);

    [[nodiscard]] std::optional<ValueType> getType(const std::string &key) const;
};

inline Storage storage;