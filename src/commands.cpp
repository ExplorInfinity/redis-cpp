#include <mutex>
#include <condition_variable>
#include <sys/socket.h>
#include <ranges>
#include <algorithm>

#include "storage.h"
#include "commands.h"
#include "utils.h"

static std::mutex storageMutex;
static std::condition_variable dataAvailableCV;

static thread_local bool MULTI_Enabled = false;
static thread_local std::vector<std::pair<std::string, TokenArray>> queuedCmds;

static const std::vector<std::string> QueueExcluded =
    { "EXEC", "DISCARD" };

void Commands::handleCmd(const int client_fd, const std::string &input) {
    auto token = RESP::parse(input);
    const auto cmd = convertToUpperCase(token.getDataType() == Token::DataType::ARRAY ? token.getArray()[0].getString() : token.getString());
    const auto &args = token.getArray();

    if (MULTI_Enabled && std::ranges::find(QueueExcluded, cmd) == QueueExcluded.end()) {
        queuedCmds.emplace_back(cmd, args);
        send(client_fd, RESP::Responses::QUEUED.c_str(), RESP::Responses::QUEUED.size(), 0);
        return;
    }

    const std::string response = commands[cmd](args);
    send(client_fd, response.c_str(), response.size(), 0);
}

std::string Commands::PING(const TokenArray &) {
    return RESP::Responses::PONG;
}

std::string Commands::ECHO(const TokenArray &args) {
    return RESP::encodeIntoBulkString(args[1].getString());
}

std::string Commands::SET(const TokenArray &args) {
    if (args.size() == 5) {
        if (!isNumericValue(args[4].getString()))
            return RESP::encodeIntoSimpleError("ERR value is not an integer or out of range");

        const float expirationTime = (args[3].getString() == "ex" ? 1000.0f : 1.0f) * static_cast<float>(std::stoi(args[4].getString()));
        storage.set<StringValue>(args[1].getString(), args[2].getString(), true, expirationTime);
    } else if (args.size() == 3) {
        storage.set<StringValue>(args[1].getString(), args[2].getString());
    } else {
        return RESP::encodeIntoSimpleError("ERR invalid number of arguments for 'set' command");
    }

    return RESP::Responses::OK;
}

std::string Commands::GET(const TokenArray &args) {
    if (const auto value = storage.get<StringValue>(args[1].getString());
        value && value->get().getType() == ValueType::String)
    {
        return RESP::encodeIntoBulkString(value->get().value);
    }

    return RESP::Responses::NULL_BULK_STRING;
}

std::string Commands::LPUSH(const TokenArray &args) {
    const auto &key = args[1].getString();
    for (int i = 2; i < args.size(); i++)
        storage.appendToList(key, args[i].getString(), true);

    const auto size = storage.sizeOfList(key);
    dataAvailableCV.notify_all();
    return RESP::encodeIntoInt(size);
}

std::string Commands::RPUSH(const TokenArray &args) {
    const auto &key = args[1].getString();
    for (int i = 2; i < args.size(); i++)
        storage.appendToList(key, args[i].getString(), false);

    const auto size = storage.sizeOfList(key);
    dataAvailableCV.notify_all();
    return RESP::encodeIntoInt(size);
}

std::string Commands::LRANGE(const TokenArray &args) {
    if (args.size() != 4)
        return RESP::encodeIntoSimpleError("ERR invalid number of arguments to 'LRANGE' command");

    if (!isNumericValue(args[2].getString()) || !isNumericValue(args[3].getString()))
        return RESP::encodeIntoSimpleError("ERR value(s) is not an integer or out of range");

    const auto arr = storage.getListCopy(
           args[1].getString(),
           std::stoi(args[2].getString()),
           std::stoi(args[3].getString())
       );
    return RESP::encodeIntoArray(arr);
}

std::string Commands::LLEN(const TokenArray &args) {
    return RESP::encodeIntoInt(storage.sizeOfList(args[1].getString()));
}

std::string Commands::LPOP(const TokenArray &args) {
    const auto &key = args[1].getString();
    if (args.size() == 3) {
        if (!isNumericValue(args[2].getString()))
            return RESP::encodeIntoSimpleError("ERR value is not an integer or out of range to 'LPOP' command");

        const int count = std::stoi(args[2].getString());
        std::vector<std::string> elements;
        elements.reserve(count);
        for (int i = 0; i < count; i++) {
            elements.push_back(storage.LPOP(key));
        }
        return RESP::encodeIntoArray(elements);
    }

    if (args.size() == 2) {
        const auto firstElement = storage.LPOP(key);
        return RESP::encodeIntoBulkString(firstElement);
    }

    return RESP::encodeIntoSimpleError("ERR invalid number of arguments to 'LPOP' command");
}

std::string Commands::BLPOP(const TokenArray &args) {
    if (args.size() != 3)
        return RESP::encodeIntoSimpleError("Err invalid number of arguments to 'BLPOP' command");

    if (!isDouble(args[2].getString()))
        return RESP::encodeIntoSimpleError("ERR value is not an integer or out of range to 'BLPOP' command");

    std::unique_lock lock(storageMutex);

    const auto &key = args[1].getString();
    const auto expirationTime = std::stod(args[2].getString());

    const auto predicate = [&] {
        return storage.sizeOfList(key) > 0;
    };

    bool lpop = false;
    if (expirationTime > 0) {
        const auto expiry = std::chrono::steady_clock::now() + std::chrono::duration<double>(expirationTime);
        lpop = dataAvailableCV.wait_until(lock, expiry, predicate);
    } else {
        dataAvailableCV.wait(lock, predicate);
        lpop = true;
    }

    return (lpop ? RESP::encodeIntoArray({ key, storage.LPOP(key) }) : RESP::Responses::NULL_ARRAY);
}

std::string Commands::TYPE(const TokenArray &args) {
    const auto &key = args[1].getString();
    const auto type = storage.getType(key);
    return RESP::encodeIntoSimpleString(type != ValueType::NIL ? ValueTypeStringMap[static_cast<int>(type)] : "none");
}

std::string Commands::XADD(const TokenArray &args) {
    const auto &key = args[1].getString();

    auto id = args[2].getString();
    const StreamValue::StreamID parsedID = StreamValue::parseStreamID(id);
    id = StreamValue::stringifyStreamID(parsedID);

    if (!Storage::isValidStreamID(parsedID)) {
        return RESP::encodeIntoSimpleError(
            id == "0-0" ?
            "ERR The ID specified in XADD must be greater than 0-0" :
            "ERR The ID specified in XADD is equal or smaller than the target stream top item"
        );
    }

    auto &streamValueRef = (
        storage.getType(key) == ValueType::NIL ?
        storage.set<StreamValue>(key, id).get() :
        storage.get<StreamValue>(key)->get()
    );

    auto &entriesRef = streamValueRef.getEntriesMapAtID(parsedID);
    for (int i = 3; i < args.size(); i += 2) {
        entriesRef[args[i].getString()] = args[i + 1].getString();
    }

    dataAvailableCV.notify_all();
    return RESP::encodeIntoBulkString(id);
}

std::string Commands::XRANGE(const TokenArray &args) {
    if (args.size() != 4)
        return RESP::encodeIntoSimpleError("ERR invalid number of arguments to 'XRANGE' command");

    const auto &key = args[1].getString(),
               &start = args[2].getString(),
               &end = args[3].getString();

    if (const auto streamValue = storage.get<StreamValue>(key)) {
        auto &value = streamValue->get();
        auto found_entries = value.getEntriesInRange(start, end);

        std::string response = std::format("*{}\r\n", found_entries.size());
        for (const auto &[id, entry] : found_entries) {
            response += "*2\r\n";
            response += RESP::encodeIntoBulkString(StreamValue::stringifyStreamID(*id));
            response += RESP::encodeMapIntoArray(*entry);
        }

        return response;
    }

    return RESP::Responses::EMPTY_ARRAY;
}

std::string Commands::XREAD(const TokenArray &args) {
    int cursor = 2;
    std::vector<std::string> responses;

    if (args[1].getString() == "block") {
        cursor += 2;
        ll time = std::stoll(args[2].getString());

        std::unique_lock lock(storageMutex);

        const auto &key = args[cursor].getString();
        const auto &id_string = args[cursor + 1].getString();
        const auto lastStreamID = Storage::getLastStreamID();
        bool waitForLatest = id_string == "$";

        const auto start = (waitForLatest ? "" : StreamValue::incrementID(args[cursor + 1].getString()));

        const auto predicate = [&] {
            if (const auto streamValue = storage.get<StreamValue>(key)) {
                auto &value = streamValue->get();
                return (waitForLatest ? *value.getLastestEntry().first > lastStreamID : !value.getEntriesInRange(start, "+").empty());
            }

            return false;
        };

        if (time != 0) {
            auto expiry = std::chrono::steady_clock::now() + std::chrono::milliseconds(time);
            const bool success = dataAvailableCV.wait_until(lock, expiry, predicate);

            if (!success) {
                return RESP::Responses::NULL_ARRAY;
            }
        } else {
            dataAvailableCV.wait(lock, predicate);
        }

        if (waitForLatest) {
            std::string response = std::format("*1\r\n*2\r\n{}*1\r\n*2\r\n", RESP::encodeIntoBulkString(key));
            const auto [id, entries] = storage.get<StreamValue>(key)->get().getLastestEntry();
            response += RESP::encodeIntoBulkString(StreamValue::stringifyStreamID(*id));
            response += RESP::encodeMapIntoArray(*entries);

            return response;
        }
    }

    const int queries = (static_cast<int>(args.size()) - cursor + 1) / 2;
    responses.reserve(queries);

    for (int i = cursor; i < queries + cursor; i++) {
        const auto &key = args[i].getString();
        const auto start = StreamValue::incrementID(args[i + queries].getString());

        if (auto streamValue = storage.get<StreamValue>(key)) {
            auto &value = streamValue->get();
            auto found_entries = value.getEntriesInRange(start, "+");

            std::string response = std::format("*2\r\n{}*{}\r\n", RESP::encodeIntoBulkString(key), found_entries.size());
            for (const auto &[id, entry] : found_entries) {
                response += "*2\r\n";
                response += RESP::encodeIntoBulkString(StreamValue::stringifyStreamID(*id));
                response += RESP::encodeMapIntoArray(*entry);
            }

            responses.push_back(response);
        }
    }

    return RESP::createRawArray(responses);
}

std::string Commands::INCR(const TokenArray &args) {
    const auto &key = args[1].getString();
    if (const auto entry = storage.get<StringValue>(key)) {
        if (const auto &value = entry->get().value; isNumericValue(value)) {
            const ll intVal = std::stoll(value) + 1;
            storage.set<StringValue>(key, std::to_string(intVal));
            return RESP::encodeIntoInt(intVal);
        }

        return RESP::encodeIntoSimpleError("ERR value is not an integer or out of range");
    }

    storage.set<StringValue>(key, "1");
    return ":1\r\n";
}

std::string Commands::MULTI(const TokenArray &args) {
    MULTI_Enabled = true;
    return RESP::Responses::OK;
}

std::string Commands::EXEC(const TokenArray &) {
    if (!MULTI_Enabled)
        return RESP::encodeIntoSimpleError("ERR EXEC without MULTI");

    MULTI_Enabled = false;
    std::vector<std::string> responses;
    responses.reserve(queuedCmds.size());
    for (const auto &[cmd, args] : queuedCmds)
        responses.emplace_back(commands[cmd](args));

    return RESP::createRawArray(responses);
}

std::string Commands::DISCARD(const TokenArray&) {
    if (!MULTI_Enabled)
        return RESP::encodeIntoSimpleError("ERR DISCARD without MULTI");

    MULTI_Enabled = false;
    queuedCmds.clear();
    return RESP::Responses::OK;
}

std::string Commands::INFO(const TokenArray &args) {
    if (args.size() >= 2 && args[1].getString() == "replication")
        return RESP::encodePairsIntoBulkString({
            { "role",  (ServerInfo.contains("--replicaof") ? "slave" : "master") },
            { "master_replid", "8371b4fb1155b71f4a04d3e1bc3e18c4a990aeeb" },
            { "master_repl_offset",  "0" },
        });

    return RESP::Responses::EMPTY_BULK_STRING;
}

std::string Commands::REPLCONF(const TokenArray &args) {
    return RESP::Responses::OK;
}

std::string Commands::PSYNC(const TokenArray &args) {
    return RESP::encodeIntoSimpleString(std::format("FULLRESYNC {} 0\r\n", "8371b4fb1155b71f4a04d3e1bc3e18c4a990aeeb"));
}

std::unordered_map<std::string, CmdFunction> commands = {
    { "PING", Commands::PING },
    { "ECHO", Commands::ECHO },
    { "SET", Commands::SET },
    { "GET", Commands::GET },
    { "LPUSH", Commands::LPUSH },
    { "RPUSH", Commands::RPUSH },
    { "LRANGE", Commands::LRANGE },
    { "LLEN", Commands::LLEN },
    { "LPOP", Commands::LPOP },
    { "BLPOP", Commands::BLPOP },
    { "TYPE", Commands::TYPE },
    { "XADD", Commands::XADD },
    { "XRANGE", Commands::XRANGE },
    { "XREAD", Commands::XREAD },
    { "INCR", Commands::INCR },
    { "MULTI", Commands::MULTI },
    { "EXEC", Commands::EXEC },
    { "DISCARD", Commands::DISCARD },
    { "INFO", Commands::INFO },
    { "REPLCONF", Commands::REPLCONF },
    { "PSYNC", Commands::PSYNC },
};

StringMap ServerInfo;

void setServerInfo(const int argc, char **argv) {
    ServerInfo = parseProgramArgs(argc, argv);
}