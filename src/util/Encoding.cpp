#include "util/Encoding.hpp"
#include <array>
#include <stdexcept>

namespace BurpTUI::Encoding {

static constexpr std::string_view kB64Chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64Encode(std::string_view input) {
    std::string out;
    out.reserve(((input.size() + 2) / 3) * 4);
    std::uint32_t val = 0;
    int           bits = -6;
    for (unsigned char c : input) {
        val  = (val << 8) + c;
        bits += 8;
        while (bits >= 0) {
            out += kB64Chars[(val >> bits) & 0x3F];
            bits -= 6;
        }
    }
    if (bits > -6) out += kB64Chars[((val << 8) >> (bits + 8)) & 0x3F];
    while (out.size() % 4) out += '=';
    return out;
}

std::string base64Decode(std::string_view input) {
    std::array<int, 256> T{};
    T.fill(-1);
    for (int i = 0; i < 64; ++i)
        T[static_cast<unsigned char>(kB64Chars[i])] = i;

    std::string out;
    std::uint32_t val  = 0;
    int           bits = -8;
    for (unsigned char c : input) {
        if (T[c] == -1) break;
        val  = (val << 6) + T[c];
        bits += 6;
        if (bits >= 0) {
            out += static_cast<char>((val >> bits) & 0xFF);
            bits -= 8;
        }
    }
    return out;
}

std::string urlEncode(std::string_view input) {
    static constexpr std::string_view unreserved =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~";
    std::string out;
    out.reserve(input.size() * 3);
    for (unsigned char c : input) {
        if (unreserved.find(c) != std::string_view::npos) {
            out += static_cast<char>(c);
        } else {
            char buf[4];
            std::snprintf(buf, sizeof(buf), "%%%02X", c);
            out += buf;
        }
    }
    return out;
}

std::string urlDecode(std::string_view input) {
    std::string out;
    out.reserve(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '%' && i + 2 < input.size()) {
            char hex[3] = { input[i+1], input[i+2], '\0' };
            out += static_cast<char>(std::strtol(hex, nullptr, 16));
            i += 2;
        } else if (input[i] == '+') {
            out += ' ';
        } else {
            out += input[i];
        }
    }
    return out;
}

std::string hexEncode(std::string_view input) {
    std::string out;
    out.reserve(input.size() * 2);
    for (unsigned char c : input) {
        char buf[3];
        std::snprintf(buf, sizeof(buf), "%02x", c);
        out += buf;
    }
    return out;
}

std::string hexDecode(std::string_view input) {
    std::string out;
    out.reserve(input.size() / 2);
    for (std::size_t i = 0; i + 1 < input.size(); i += 2) {
        char hex[3] = { input[i], input[i+1], '\0' };
        out += static_cast<char>(std::strtol(hex, nullptr, 16));
    }
    return out;
}

} // namespace BurpTUI::Encoding
