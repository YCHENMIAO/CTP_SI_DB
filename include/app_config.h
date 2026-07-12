#pragma once

#include <cstdio>
#include <string>

struct AccountConfig {
    std::string appid;
    std::string authcode;
    std::string product;
    std::string brokerId;
    std::string userId;
    std::string passwd;
    std::string marketFront;
    std::string tradeFront;
};

template <size_t N>
void copyCtpString(char (&dst)[N], const std::string& src) {
    std::snprintf(dst, N, "%s", src.c_str());
}
