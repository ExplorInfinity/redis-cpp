#pragma once

#include <string>

namespace TCP {
    int connectToServer(std::string IP, int PORT);
    void closeConnection(int sock);
}