#pragma once

#include <vector>
#include <string>
#include <format>
#include <unordered_map>

namespace RESP {
    constexpr std::string CRLF = "\r\n";

    namespace Responses {
        constexpr std::string OK = "+OK\r\n";
        constexpr std::string PONG = "+PONG\r\n";
        constexpr std::string QUEUED = "+QUEUED\r\n";
        constexpr std::string NULL_ARRAY = "*-1\r\n";
        constexpr std::string EMPTY_ARRAY = "*0\r\n";
        constexpr std::string NULL_BULK_STRING = "$-1\r\n";
    };

    class Token {
    public:
        enum class DataType { NIL, INTEGER, S_STRING, B_STRING, ARRAY };
    private:
        DataType type = DataType::NIL;
        long long m_int = -1;
        std::string m_string;
        std::vector<Token> m_arr;

    public:
        Token() = default;
        explicit Token(DataType type);

        [[nodiscard]] DataType getDataType() const;

        long long& getInt();
        std::string& getString();
        std::vector<Token>& getArray();

        [[nodiscard]] long long getInt() const;
        [[nodiscard]] std::string getString() const;
        [[nodiscard]] std::vector<Token> getArray() const;
    };

    Token parse(const std::string &s);
    Token parse(const std::string &s, int &pos);

    Token parseInteger(const std::string &s, int &pos);
    Token parseSimpleString(const std::string &s, int &pos);
    Token parseBulkString(const std::string &s, int &pos);
    Token parseArray(const std::string &s, int &pos);

    std::string encodeIntoInt(long long i);
    std::string encodeIntoSimpleString(const std::string &s);
    std::string encodeIntoBulkString(const std::string &s);
    std::string encodeIntoArray(const std::vector<std::string> &v);
    std::string encodeMapIntoArray(const std::unordered_map<std::string, std::string> &mp);
    std::string encodeIntoSimpleError(const std::string &s);

    std::string createRawArray(const std::vector<std::string> &v);
}

