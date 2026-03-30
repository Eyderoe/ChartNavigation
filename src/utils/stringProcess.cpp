#include "stringProcess.hpp"

std::string toHex (const char *data, const int length) {
    std::string base;
    base.reserve(length * 3);
    for (int i = 0; i < length; i++)
        base += std::format("{:02X} ", data[i]);
    return base;
}
