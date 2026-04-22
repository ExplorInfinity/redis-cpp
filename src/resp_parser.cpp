#include "resp_parser.h"

#include <iostream>
#include <string>
#include <ranges>
#include <algorithm>

#define debug 0

/* RESP Token Class */
RESP::Token::Token(const DataType type) : type(type) {}

RESP::Token::DataType RESP::Token::getDataType() const {
    return type;
}

long long& RESP::Token::getInt() {
    return m_int;
}

std::string& RESP::Token::getString() {
    return m_string;
}

std::vector<RESP::Token> &RESP::Token::getArray() {
    return m_arr;
}

const long long& RESP::Token::getInt() const {
    return m_int;
}

const std::string& RESP::Token::getString() const {
    return m_string;
}

const std::vector<RESP::Token> &RESP::Token::getArray() const {
    return m_arr;
}

/* RESP Parser */
RESP::Token RESP::parse(const std::string &s) {
    int pos = 0;
    std::string lowerCase = s;
    std::ranges::transform(
        lowerCase, lowerCase.begin(),
        [](unsigned char c) {
            return std::tolower(c);
        });
    return parse(lowerCase, pos);
}

RESP::Token RESP::parse(const std::string &s, int &pos) {
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
            std::cout << "Invalid RESP string!\n";
            return Token{};
    }
}

RESP::Token RESP::parseInteger(const std::string &s, int &pos) {
    const int endIndex = static_cast<int>(s.find(CRLF, pos));
    Token t(Token::DataType::INTEGER);
    t.getInt() = std::stoi(s.substr(pos + 1, endIndex - (pos + 1)));
    pos = endIndex + CRLF.size();

#if debug
    std::cout << t.getInt() << std::endl;
#endif

    return t;
}

RESP::Token RESP::parseSimpleString(const std::string &s, int &pos) {
    const int endIndex = static_cast<int>(s.find(CRLF, pos)),
              length = endIndex - (pos + 1);
    Token t(Token::DataType::S_STRING);
    t.getString() = s.substr(pos + 1, length);
    pos = endIndex + CRLF.size();

#if debug
    std::cout << t.getString() << std::endl;
#endif

    return t;
}

RESP::Token RESP::parseBulkString(const std::string &s, int &pos) {
    const int endIndex = static_cast<int>(s.find(CRLF, pos)),
              length = endIndex - (pos + 1);
    const int size = std::stoi(s.substr(pos + 1, endIndex - (pos + 1)));
    pos = endIndex + CRLF.size();

    Token t(Token::DataType::B_STRING);
    t.getString() = s.substr(pos, size);
    pos += size + CRLF.size();

#if debug
    std::cout << t.getString() << std::endl;
#endif

    return t;
}

RESP::Token RESP::parseArray(const std::string &s, int &pos) {
    const int endIndex = static_cast<int>(s.find(CRLF, pos));
    const int count = std::stoi(s.substr(pos + 1, endIndex - (pos + 1)));
    pos = endIndex + CRLF.size();

#if debug
    std::cout << "Array: " << count << std::endl;
#endif

    Token t(Token::DataType::ARRAY);
    auto &array = t.getArray();
    array.reserve(count);
    for (int i = 0; i < count; i++) {
        array.push_back(parse(s, pos));
    }

    return t;
}

std::string RESP::encodeIntoInt(long long i) {
    return std::format(":{}\r\n", i);
}

std::string RESP::encodeIntoBulkString(const std::string &s) {
    return std::format("${}\r\n{}\r\n", s.size(), s);
}