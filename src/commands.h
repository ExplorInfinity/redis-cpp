#pragma once

#include <vector>
#include <functional>
#include <unordered_map>

#include "resp_parser.h"
#include "utils.h"

using RESP::Token;
using TokenArray = std::vector<Token>;
using CmdFunction = std::string(*)(const TokenArray &);
using StringMap = std::unordered_map<std::string, std::string>;

extern thread_local int curr_client_fd;

namespace Commands {
    void handleCmd(int client_fd, const std::string &input);

    std::string PING(const TokenArray &args);
    std::string ECHO(const TokenArray &args);
    std::string SET(const TokenArray &args);
    std::string GET(const TokenArray &args);
    std::string LPUSH(const TokenArray &args);
    std::string RPUSH(const TokenArray &args);
    std::string LRANGE(const TokenArray &args);
    std::string LLEN(const TokenArray &args);
    std::string LPOP(const TokenArray &args);
    std::string BLPOP(const TokenArray &args);
    std::string TYPE(const TokenArray &args);
    std::string XADD(const TokenArray &args);
    std::string XRANGE(const TokenArray &args);
    std::string XREAD(const TokenArray &args);
    std::string INCR(const TokenArray &args);
    std::string MULTI(const TokenArray &args);
    std::string EXEC(const TokenArray &args);
    std::string DISCARD(const TokenArray& args);
    std::string INFO(const TokenArray& args);
    std::string REPLCONF(const TokenArray &args);
    std::string PSYNC(const TokenArray &args);
};

extern std::unordered_map<std::string, CmdFunction> commands;
extern StringMap ServerInfo;

void setServerInfo(int argc, char **argv);