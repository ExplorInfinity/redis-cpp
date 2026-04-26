#pragma once

#include <string>
#include <map>
#include <unordered_map>
#include <optional>
#include <utility>
#include <vector>
#include <memory>

#include "resp_parser.h"
#include "timer.h"

enum class ValueType { NIL, String, Stream, List };
inline std::vector<std::string> ValueTypeStringMap = { "nil", "string", "stream", "list" };

using ll = long long;

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

    [[nodiscard]] ValueType getType() const override { return ValueType::String; };
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

    [[nodiscard]] ValueType getType() const override { return ValueType::List; };
};

class StreamValue : public Value {
public:
    using StreamID = std::pair<ll, ll>;
    using StreamEntry = std::unordered_map<std::string, std::string>;

    std::map<StreamID, StreamEntry> entries;

    static StreamID parseStreamID(const std::string &s);
    static std::string stringifyStreamID(const StreamID &id);
    static std::string incrementID(const std::string &s);

    StreamValue() = default;

    explicit StreamValue(const std::string &id)
    {
        setID(parseStreamID(id));
    };

    explicit StreamValue(const std::string &id, const bool expires, const float expirationTime)
        : Value(expires, expirationTime)
    {
        setID(parseStreamID(id));
    };

    [[nodiscard]] ValueType getType() const override { return ValueType::Stream; };

    StreamEntry& setID(const StreamID &id);
    StreamEntry& getEntriesMapAtID(const StreamID &id);

    std::vector<std::pair<const StreamID*, StreamEntry*>> getEntriesInRange(const std::string &start, const std::string &end);
};


class Storage {
    std::unordered_map<std::string, std::unique_ptr<Value>> kvStorage;
    static StreamValue::StreamID lastStreamID;
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

    /* Stream Helper functions */
    static ll getValidStreamIDSqNum(ll ms);
    static void setCurrStreamID(ll ms, ll sq);
    static void setCurrStreamID(const StreamValue::StreamID &id);
    [[nodiscard]] static bool isValidStreamID(ll ms, ll sq);
    [[nodiscard]] static bool isValidStreamID(const std::pair<ll, ll> &id);

    [[nodiscard]] ValueType getType(const std::string &key) const;
};

inline Storage storage;