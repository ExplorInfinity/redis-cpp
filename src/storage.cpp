#include "storage.h"

#include <iostream>
#include <string>
#include <unordered_map>
#include <optional>
#include <functional>
#include <memory>

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

bool Storage::containsList(const std::string &key) const {
    const auto it = kvStorage.find(key);
    return (it != kvStorage.end() && it->second->getType() == ValueType::List && !dynamic_cast<ListValue*>(it->second.get())->values.empty());
}

std::optional<ValueType> Storage::getType(const std::string &key) const {
    const auto it = kvStorage.find(key);

    if (it == kvStorage.end())
        return std::nullopt;

    return it->second->getType();
}
