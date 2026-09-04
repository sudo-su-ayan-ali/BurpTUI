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
    req.rawOverride = std::string(raw);

    std::string str(raw);
    std::istringstream stream(str);
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
