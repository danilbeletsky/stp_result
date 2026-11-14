#pragma once

#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>

namespace pvz::text {

inline std::string trim(std::string_view value) {
    std::size_t begin = 0;
    std::size_t end = value.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return std::string(value.substr(begin, end - begin));
}

// Количество символов в строке UTF-8 (а не байт): так же считает VARCHAR(n) в PostgreSQL.
inline std::size_t utf8Length(std::string_view value) {
    std::size_t count = 0;
    for (unsigned char c : value) {
        if ((c & 0xC0U) != 0x80U) {
            ++count;
        }
    }
    return count;
}

inline std::string toUpperAscii(std::string_view value) {
    std::string result(value);
    for (char& c : result) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return result;
}

} // namespace pvz::text
