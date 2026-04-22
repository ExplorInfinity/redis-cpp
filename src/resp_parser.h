#pragma once

#include <vector>
#include <string>
#include <format>

namespace Responses {
    constexpr const char* NullBulkString = "$-1\r\n";
    constexpr const char* OK = "+OK\r\n";
}

namespace RESP {
    constexpr std::string CRLF = "\r\n";

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

        [[nodiscard]] const long long& getInt() const;
        [[nodiscard]] const std::string& getString() const;
        [[nodiscard]] const std::vector<Token>& getArray() const;
    };

    Token parse(const std::string &s);
    Token parse(const std::string &s, int &pos);

    Token parseInteger(const std::string &s, int &pos);
    Token parseSimpleString(const std::string &s, int &pos);
    Token parseBulkString(const std::string &s, int &pos);
    Token parseArray(const std::string &s, int &pos);

    std::string encodeIntoBulkString(const std::string &s);
}

