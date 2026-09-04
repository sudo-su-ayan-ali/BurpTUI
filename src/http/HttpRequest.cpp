#include "http/HttpRequest.hpp"
#include <sstream>

namespace BurpTUI {

std::string HttpRequest::header(std::string_view name) const {
    for (const auto& [k, v] : headers)
        if (k == name) return v;
    return {};
}

std::string HttpRequest::serialize() const {
    if (!rawOverride.empty()) {
        return rawOverride;
    }
    std::ostringstream oss;
    oss << method << " " << url << " " << version << "\r\n";
    for (const auto& [k, v] : headers)
        oss << k << ": " << v << "\r\n";
    oss << "\r\n" << body;
    return oss.str();
}

HttpRequest ParseRawHttpRequest(std::string_view raw) {
    HttpRequest req;

    std::string str(raw);
    // Sanitize any (HTTPS) or (HTTP) in Host lines
    auto p1 = str.find(" (HTTPS)");
    if (p1 != std::string::npos) str.erase(p1, 8);
    auto p2 = str.find(" (HTTP)");
    if (p2 != std::string::npos) str.erase(p2, 7);

    // Normalize CRLF
    std::string normalized;
    normalized.reserve(str.size() + 16);
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\r') continue;
        if (str[i] == '\n') {
            normalized += "\r\n";
        } else {
            normalized += str[i];
        }
    }
    // Ensure header section has \r\n\r\n
    if (normalized.find("\r\n\r\n") == std::string::npos) {
        if (normalized.ends_with("\r\n")) {
            normalized += "\r\n";
        } else {
            normalized += "\r\n\r\n";
        }
    }
    req.rawOverride = normalized;

    std::istringstream stream(normalized);
    std::string line;

    // First line: METHOD URL VERSION
    if (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream lineStream(line);
        lineStream >> req.method >> req.url >> req.version;
        if (req.method.empty()) req.method = "GET";
        if (req.url.empty()) req.url = "/";
        if (req.version.empty()) req.version = "HTTP/1.1";
    }

    // Headers until blank line
    bool inBody = false;
    std::ostringstream bodyStream;
    while (std::getline(stream, line)) {
        if (!inBody) {
            if (line == "\r" || line.empty()) {
                inBody = true;
                continue;
            }
            if (!line.empty() && line.back() == '\r') line.pop_back();
            auto colonPos = line.find(':');
            if (colonPos != std::string::npos) {
                std::string key = line.substr(0, colonPos);
                std::string val = line.substr(colonPos + 1);
                // Trim leading whitespace on value
                auto firstNonSpace = val.find_first_not_of(" \t");
                if (firstNonSpace != std::string::npos) {
                    val = val.substr(firstNonSpace);
                } else {
                    val.clear();
                }
                req.headers.push_back({std::move(key), std::move(val)});
            }
        } else {
            bodyStream << line << "\n";
        }
    }
    req.body = bodyStream.str();
    // Trim trailing newline added by loop if raw didn't end with extra newline
    if (!req.body.empty() && !str.ends_with("\n")) {
        req.body.pop_back();
    }

    return req;
}

} // namespace BurpTUI
