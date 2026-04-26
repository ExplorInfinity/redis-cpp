#include "storage.h"

#include <iostream>
#include <string>
#include <unordered_map>
#include <optional>
#include <functional>
#include <memory>
#include <climits>

#include "timer.h"

#define debug 0

void Storage::appendToList(const std::string &key, const std::string &value, const bool prepend) {
    const auto it = kvStorage.find(key);
    if (it == kvStorage.end()) {
        set<ListValue>(key, value);
        return;
    }

    if (it->second->getType() != ValueType::List)
        return;

    auto *list = dynamic_cast<ListValue*>(it->second.get());
    if (prepend)
        list->values.insert(list->values.begin(), value);
    else
        list->values.push_back(value);
}

std::vector<std::string> Storage::getListCopy(const std::string &key, int start, int stop) const {

    const auto it = kvStorage.find(key);
    if (it == kvStorage.end() || it->second->getType() != ValueType::List)
        return {};

    const auto *list = dynamic_cast<ListValue*>(it->second.get());
    const auto &values = list->values;
    const int size = static_cast<int>(values.size());

    if (values.empty())
        return {};

    start = std::max(0, (start + size) % size);
    stop = std::max(0, (std::min(stop, size - 1) + size) % size);

    if (start >= values.size())
        return {};

    std::vector<std::string> res(stop - start + 1);
    std::copy(values.begin() + start, values.begin() + (stop + 1), res.begin());

    return res;
}

std::size_t Storage::sizeOfList(const std::string &key) const {
    const auto it = kvStorage.find(key);
    if (it == kvStorage.end() || it->second->getType() != ValueType::List)
        return 0;

    const auto *list = dynamic_cast<ListValue*>(it->second.get());
    return list->values.size();
}

std::string Storage::RPOP(const std::string &key) {
    const auto it = kvStorage.find(key);
    if (it == kvStorage.end() || it->second->getType() != ValueType::List)
        return "";

    auto *list = dynamic_cast<ListValue*>(it->second.get());
    auto &values = list->values;

    const auto temp = values.back();
    values.pop_back();

    if (values.empty())
        kvStorage.erase(key);

    return temp;
}

std::string Storage::LPOP(const std::string &key) {
    const auto it = kvStorage.find(key);
    if (it == kvStorage.end() || it->second->getType() != ValueType::List)
        return "";

    auto *list = dynamic_cast<ListValue*>(it->second.get());
    auto &values = list->values;

    const auto temp = values.front();
    values.erase(values.begin());

    if (values.empty())
        kvStorage.erase(key);

    return temp;
}

ll Storage::getValidStreamIDSqNum(const ll ms) {
    const auto [lms, lsq] = Storage::lastStreamID;
    return lms > ms ? -1 : (lsq + 1) * (lms == ms);
}

bool Storage::containsList(const std::string &key) const {
    const auto it = kvStorage.find(key);
    return (it != kvStorage.end() && it->second->getType() == ValueType::List && !dynamic_cast<ListValue*>(it->second.get())->values.empty());
}

StreamValue::StreamID Storage::lastStreamID{};

void Storage::setCurrStreamID(const ll ms, const ll sq) {
    auto &[lms, lsq] = lastStreamID;
    if (ms > lms) {
        lms = ms;
        lsq = 0;
    } else if (ms == lms && sq > lsq) {
        lsq = sq;
    }
}

void Storage::setCurrStreamID(const StreamValue::StreamID &id) {
    setCurrStreamID(id.first, id.second);
}

bool Storage::isValidStreamID(const ll ms, const ll sq) {
    const auto [lms, lsq] = lastStreamID;
    return (ms > lms || (ms == lms && sq > lsq));
}

bool Storage::isValidStreamID(const StreamValue::StreamID &id) {
    return isValidStreamID(id.first, id.second);
}

ValueType Storage::getType(const std::string &key) const {
    const auto it = kvStorage.find(key);

    if (it == kvStorage.end())
        return ValueType::NIL;

    return it->second->getType();
}

StreamValue::StreamID StreamValue::parseStreamID(const std::string &s) {
    ll ms, sq;

    if (s == "*") {
        ms = getCurrTimeInMilliSecs();
        sq = Storage::getValidStreamIDSqNum(ms);
        return { ms, sq };
    }

    const auto index = s.find('-');
    if (index == std::string::npos) {
        ms = std::stoll(s);
        sq = Storage::getValidStreamIDSqNum(ms);
        return { ms, sq };
    }

    const auto msPart = s.substr(0, index);
    const auto sqPart = s.substr(index + 1);

    ms = (msPart == "*" ? getCurrTimeInMilliSecs() : std::stoll(msPart));
    sq = (sqPart == "*" || sqPart.empty()) ? Storage::getValidStreamIDSqNum(ms) : std::stoll(sqPart);

    return { ms, sq };
}

std::string StreamValue::stringifyStreamID(const StreamID &id) {
    return std::format("{}-{}", id.first, id.second);
}

std::string StreamValue::incrementID(const std::string &s) {
    const auto index = s.find('-');

    if (index == std::string::npos)
        return std::to_string(std::stoll(s) + 1ll);

    return s.substr(0, index + 1) + std::to_string(std::stoll(s.substr(index + 1)) + 1ll);
}

StreamValue::StreamEntry& StreamValue::setID(const StreamID &id) {
    Storage::setCurrStreamID(id);
    return entries[id];
}

StreamValue::StreamEntry& StreamValue::getEntriesMapAtID(const StreamID &id) {
    const auto it = entries.find(id);

    if (it == entries.end())
        return setID(id);

    return it->second;
}

std::vector<std::pair<const StreamValue::StreamID*, StreamValue::StreamEntry*>> StreamValue::getEntriesInRange(const std::string &start, const std::string &end) {
    std::vector<std::pair<const StreamID*, StreamEntry*>> found_entries;

    auto parseStrID = [] (const std::string &s, StreamID &id) {
        if (s == "-") {
            id.first = id.second = 0;
        } else if (s == "+") {
            id.first = id.second = LLONG_MAX;
        } else if (const auto index = s.find('-'); index != std::string::npos) {
            id.first = std::stoll(s.substr(0, index));
            id.second = std::stoll(s.substr(index + 1));
        } else {
            id.first = std::stoll(s);
        }
    };

    StreamID startID{0, 0}, endID{0, LLONG_MAX};
    parseStrID(start, startID);
    parseStrID(end, endID);

    const auto itStart = entries.lower_bound(startID);
    const auto itEnd = entries.upper_bound(endID);
    for (auto it = itStart; it != itEnd; ++it) {
        found_entries.emplace_back(&it->first, &it->second);
    }

    return found_entries;
}