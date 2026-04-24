#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "resp_parser.h"
#include "utils.h"
#include "storage.h"

#define debug 0

using RESP::Token;

std::mutex storageMutex;
std::condition_variable dataAvailableCV;

void handleCmd(const std::string &input, const int client_fd) {
    auto token = RESP::parse(input);
    const auto &cmd = (token.getDataType() == Token::DataType::ARRAY ?  token.getArray()[0].getString() : token.getString());
    const auto &args = token.getArray();

    if (cmd == "ping") {
        send(client_fd, Responses::PONG, strlen(Responses::PONG), 0);
    } else if (cmd == "echo") {
        const auto &s = args[1].getString();
        const std::string response = RESP::encodeIntoBulkString(s);
        send(client_fd, response.c_str(), strlen(response.c_str()), 0);
    } else if (cmd == "set") {
        if (args.size() == 5) {
            const float expirationTime = (args[3].getString() == "ex" ? 1000.0f : 1.0f) * static_cast<float>(std::stoi(args[4].getString()));
            storage.set<StringValue>(args[1].getString(), args[2].getString(), true, expirationTime);
        } else if (args.size() == 3) {
            storage.set<StringValue>(args[1].getString(), args[2].getString());
        }

        send(client_fd, Responses::OK, strlen(Responses::OK), 0);
    } else if (cmd == "get") {
        if (const auto value = storage.get<StringValue>(args[1].getString())) {
            const std::string response = RESP::encodeIntoBulkString(value->get().value);
            send(client_fd, response.c_str(), response.size(), 0);
        } else send(client_fd, Responses::NullBulkString, strlen(Responses::NullBulkString), 0);
    } else if (cmd == "rpush" || cmd == "lpush") {
        const auto &key = args[1].getString();
        for (int i = 2; i < args.size(); i++) {
            storage.appendToList(key, args[i].getString(), cmd == "lpush");
        }
        const std::string response = RESP::encodeIntoInt(storage.sizeOfList(key));
        dataAvailableCV.notify_all();
        send(client_fd, response.c_str(), response.size(), 0);
    } else if (cmd == "lrange") {
        const auto arr = storage.getListCopy(
            args[1].getString(),
            std::stoi(args[2].getString()),
            std::stoi(args[3].getString())
        );
        const std::string response = RESP::encodeIntoArray(arr);
        send(client_fd, response.c_str(), response.size(), 0);
    } else if (cmd == "llen") {
        const std::string response = RESP::encodeIntoInt(storage.sizeOfList(args[1].getString()));
        send(client_fd, response.c_str(), response.size(), 0);
    } else if (cmd == "lpop") {
        const auto &key = args[1].getString();
        if (args.size() == 3) {
            const int count = std::stoi(args[2].getString());
            std::vector<std::string> elements;
            elements.reserve(count);
            for (int i = 0; i < count; i++) {
                elements.push_back(storage.LPOP(key));
            }
            const std::string response = RESP::encodeIntoArray(elements);
            send(client_fd, response.c_str(), response.size(), 0);
        } else {
            const std::string firstElement = storage.LPOP(key);
            const std::string response = (firstElement.empty() ? Responses::NullBulkString : RESP::encodeIntoBulkString(firstElement));
            send(client_fd, response.c_str(), response.size(), 0);
        }
    } else if (cmd == "blpop") {
        if (args.size() != 3) {
            std::cout << "Invalid number of arguments to command BLPOP" << std::endl;
            std::cout << "BLPOP <list_key> <expiration_time>" << std::endl;
            return;
        }

        std::unique_lock<std::mutex> lock(storageMutex);

        auto &key = args[1].getString();
        auto expirationTime = std::stod(args[2].getString());

        std::cout << expirationTime << std::endl;

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

        const std::string response = (lpop ? RESP::encodeIntoArray({ key, storage.LPOP(key) }) : Responses::NullArray);
        lock.unlock();
        send(client_fd, response.c_str(), response.size(), 0);
    } else if (cmd == "type") {
        const auto &key = args[1].getString();
        const auto type = storage.getType(key);
        const std::string response = RESP::encodeIntoSimpleString(type ? ValueTypeStringMap[static_cast<int>(*type)] : "none");
        send(client_fd, response.c_str(), response.size(), 0);
    } else if (cmd == "xadd") {
        const auto &key = args[1].getString();
        auto id = args[2].getString();
        const auto parsedID = StreamValue::parseStreamID(id);
        id = std::format("{}-{}", parsedID.first, parsedID.second);

        if (!Storage::isValidStreamID(parsedID)) {
            const std::string response = RESP::encodeIntoSimpleError(
                id == "0-0" ?
                "ERR The ID specified in XADD must be greater than 0-0" :
                "ERR The ID specified in XADD is equal or smaller than the target stream top item"
            );
            send(client_fd, response.c_str(), response.size(), 0);
            return;
        }

        auto &valueContainer = storage.set<StreamValue>(key, args[2].getString()).get();
        for (int i = 3; i < args.size(); i += 2) {
            valueContainer.kv_pairs[args[i].getString()] = args[i + 1].getString();
        }

        const std::string response = RESP::encodeIntoBulkString(id);
        send(client_fd, response.c_str(), response.size(), 0);
    }
}

void handle_client(const int client_fd) {
    std::string inputBuffer;

    char buffer[1024];
    while (true) {
        auto bytes_received = recv(client_fd, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
#if debug
        std::cerr << "Client Disconnected\n";
#endif
            break;
        }

        inputBuffer.append(buffer, bytes_received);

#if debug
        printRaw(inputBuffer);
#endif

        handleCmd(inputBuffer, client_fd);
        inputBuffer.clear();
    }

#if debug
    std::cout << "Client Disconnected\n";
#endif

    close(client_fd);
}

int main(int argc, char **argv) {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    // Testing String
    // auto token = RESP::parse("*4\r\n:10001\r\n+Hello Bye!\r\n$12\r\nMaybe Maybe!\r\n*2\r\n+Test 1\r\n+Test 2\r\n");

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Failed to create server socket\n";
        return 1;
    }

    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        std::cerr << "setsockopt failed\n";
        return 1;
    }

    struct sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(6379);

    if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
        std::cerr << "Failed to bind to port 6379\n";
        return 1;
    }

    int connection_backlog = 5;
    if (listen(server_fd, connection_backlog) != 0) {
        std::cerr << "listen failed\n";
        return 1;
    }

    struct sockaddr_in client_addr{};
    int client_addr_len = sizeof(client_addr);
    std::cout << "Waiting for a client to connect...\n";

    while (true) {
        int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
        std::cout << "Client connected\n";

        std::thread(handle_client, client_fd).detach();
    }

    close(server_fd);
    return 0;
}
