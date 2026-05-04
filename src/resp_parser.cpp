#include "resp_parser.h"

#include <iostream>
#include <string>
#include <ranges>
#include <algorithm>

#include "utils.h"

#define debug 0

static int CRLF_Size = RESP::CRLF.size();
static RESP::Token EmptyToken{};

namespace RESP {
    /* RESP Token Class */
    Token::Token(const DataType type) : type(type) {}

    Token::DataType Token::getDataType() const {
        return type;
    }

    long long& Token::getInt() {
        return m_int;
    }

    std::string& Token::getString() {
        return m_string;
    }

    std::vector<Token> &Token::getArray() {
        return m_arr;
    }

    long long Token::getInt() const {
        return m_int;
    }

    std::string Token::getString() const {
        return m_string;
    }

    std::vector<Token> Token::getArray() const {
        return m_arr;
    }

    /* RESP Parser */
    Token parse(const std::string &s) {
        int pos = 0;
        return parse(s, pos).first;
    }

    std::pair<int, std::vector<Token>> partialParse(const std::string &s) {
        int pos = 0, lpos = 0;
        std::vector<Token> tokens;

        while (pos < s.size()) {
            lpos = pos;
            try {
                auto [token, status] = parse(s, pos);
                if (status != ParseStatus::OK) {
                    std::cout << (status == ParseStatus::ERROR ? "ERROR" : "INCOMPLETE") << std::endl;
                    return std::make_pair( lpos, tokens );
                }
                token.cmdStr = s.substr(lpos, pos - lpos);
                tokens.push_back(std::move(token));
            } catch (...) {
                return std::make_pair(lpos, tokens);
            }
        }
        return std::make_pair(pos, tokens);
    }

    ReturnToken parse(const std::string &s, int &pos) {
        if (pos >= s.size())
            return { EmptyToken, ParseStatus::INCOMPLETE };

        switch (s[pos]) {
            case ':':
                return parseInteger(s, pos);
            case '+':
                return parseSimpleString(s, pos);
            case '$':
                return parseBulkString(s, pos);
            case '*':
                return parseArray(s, pos);
            default:
                return { EmptyToken, ParseStatus::ERROR };
        }
    }

    ReturnToken parseInteger(const std::string &s, int &pos) {
        const int endIndex = static_cast<int>(s.find(CRLF, pos));

        if (endIndex == std::string::npos) {
            return { EmptyToken, ParseStatus::INCOMPLETE };
        }

        int val;
        try {
            val = std::stoi(s.substr(pos + 1, endIndex - (pos + 1)));
        } catch (...) {
            return { EmptyToken, ParseStatus::ERROR };
        }

        Token t(Token::DataType::INTEGER);
        t.getInt() = val;
        pos = endIndex + CRLF_Size;

        return { t, ParseStatus::OK };
    }

    ReturnToken parseSimpleString(const std::string &s, int &pos) {
        const int endIndex = static_cast<int>(s.find(CRLF, pos));

        if (endIndex == std::string::npos) {
            return { EmptyToken, ParseStatus::INCOMPLETE };
        }

        Token t(Token::DataType::S_STRING);
        t.getString() = s.substr(pos + 1, endIndex - (pos + 1));
        pos = endIndex + CRLF_Size;

        return { t, ParseStatus::OK };
    }

    ReturnToken parseBulkString(const std::string &s, int &pos) {
        const int endIndex = static_cast<int>(s.find(CRLF, pos));

        if (endIndex == std::string::npos) {
            return { EmptyToken, ParseStatus::INCOMPLETE };
        }

        int size;
        try {
            size = std::stoi(s.substr(pos + 1, endIndex - (pos + 1)));
        } catch (...) {
            return { EmptyToken, ParseStatus::ERROR };
        }

        pos = endIndex + CRLF_Size;

        if (pos + size + CRLF_Size > s.size())
            return { EmptyToken, ParseStatus::INCOMPLETE };

        if (s.substr(pos + size, CRLF_Size) != CRLF)
            return { EmptyToken, ParseStatus::ERROR };

        Token t(Token::DataType::B_STRING);
        t.getString() = s.substr(pos, size);
        pos += size + CRLF_Size;

        return { t, ParseStatus::OK };
    }

    ReturnToken parseArray(const std::string &s, int &pos) {
        const int endIndex = static_cast<int>(s.find(CRLF, pos));
        if (endIndex == std::string::npos) {
            return { EmptyToken, ParseStatus::INCOMPLETE };
        }

        int count;
        try {
            count = std::stoi(s.substr(pos + 1, endIndex - (pos + 1)));
        } catch (...) {
            return { EmptyToken, ParseStatus::ERROR };
        }

        pos = endIndex + CRLF_Size;

        Token t(Token::DataType::ARRAY);
        auto &array = t.getArray();
        array.reserve(count);

        for (int i = 0; i < count; i++) {
            auto [token, status] = parse(s, pos);
            if (status != ParseStatus::OK)
                return { EmptyToken, status };

            array.push_back(std::move(token));
        }

        return { t, ParseStatus::OK };
    }

    /* Encoding Helper Functions */
    std::string encodeIntoInt(long long i) {
        return std::format(":{}\r\n", i);
    }

    std::string encodeIntoSimpleString(const std::string &s) {
        return std::format("+{}\r\n", s);
    }

    std::string encodeIntoBulkString(const std::string &s) {
        return std::format("${}\r\n{}\r\n", s.size(), s);
    }

    std::string encodeIntoArray(const std::vector<std::string> &v) {
        std::string s = std::format("*{}\r\n", v.size());
        for (const auto &value : v)
            s += encodeIntoBulkString(value);
        return s;
    }

    std::string encodeMapIntoArray(const std::unordered_map<std::string, std::string> &mp) {
        std::string s = std::format("*{}\r\n", mp.size() * 2);
        for (const auto &[key, value] : mp) {
            s += encodeIntoBulkString(key);
            s += encodeIntoBulkString(value);
        }
        return s;
    }

    std::string encodeIntoSimpleError(const std::string &s) {
        return std::format("-{}\r\n", s);
    }

    std::string createRawArray(const std::vector<std::string> &v) {
        std::string s = std::format("*{}\r\n", v.size());
        for (const auto &value : v)
            s += value;
        return s;
    }

    std::string encodePairsIntoBulkString(const std::vector<std::pair<std::string, std::string>> &kv_pairs) {
        std::string response;
        for (const auto &[key, value] : kv_pairs)
            response += std::format("{}:{}\r\n", key, value);
        return encodeIntoBulkString(response);
    }

    std::string encode(const Token &token) {
        switch (token.getDataType()) {
            case Token::DataType::ARRAY: {
                auto s = std::format("*{}\r\n", token.getArray().size());
                for (const auto &t : token.getArray()) {
                    s += encode(t);
                }
                return s;
            }
            case Token::DataType::B_STRING:
                return std::format("${}\r\n{}\r\n", token.getString().size(), token.getString());
            case Token::DataType::INTEGER:
                return std::format(":{}\r\n", token.getInt());
            case Token::DataType::S_STRING:
                return std::format("+{}\r\n", token.getString());
            case Token::DataType::NIL:
            default:
                return Responses::NULL_ARRAY;
        }
    }
}