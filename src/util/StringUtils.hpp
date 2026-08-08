#pragma once
#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace BurpTUI::StringUtils {

inline std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return s;
}

inline std::string trim(std::string_view sv) {
    auto b = sv.find_first_not_of(" \t\r\n");
    if (b == std::string_view::npos) return {};
    auto e = sv.find_last_not_of(" \t\r\n");
    return std::string(sv.substr(b, e - b + 1));
}

inline std::vector<std::string> split(std::string_view sv, char delim) {
    std::vector<std::string> out;
    std::size_t start = 0;
    while (true) {
        auto pos = sv.find(delim, start);
        out.emplace_back(sv.substr(start, pos - start));
        if (pos == std::string_view::npos) break;
        start = pos + 1;
    }
    return out;
}

} // namespace BurpTUI::StringUtils
