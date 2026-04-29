#pragma once

#include <string>
#include <unordered_map>

void printRaw(const std::string &s);
bool isNumericValue(const std::string &s);
std::string convertToUpperCase(std::string s);
bool isDouble(const std::string &s);

bool isFlag(const std::string &s);

using StringMap = std::unordered_map<std::string, std::string>;
StringMap parseProgramArgs(int argc, char **argv);
