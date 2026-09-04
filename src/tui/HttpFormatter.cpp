#include "tui/HttpFormatter.hpp"
#include <ftxui/dom/elements.hpp>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace BurpTUI {

using namespace ftxui;

std::string SanitizeText(std::string_view input, std::size_t maxLen) {
    std::string out;
    out.reserve(std::min(input.size(), maxLen));
    std::size_t count = 0;
    for (unsigned char c : input) {
        if (count >= maxLen) break;
        if (c == '\r') continue;
        if (c == '\n' || c == '\t' || (c >= 32 && c < 127)) {
            out += static_cast<char>(c);
            count++;
        } else if (c >= 128) {
            out += static_cast<char>(c);
            count++;
        } else {
            out += '.';
            count++;
        }
    }
    return out;
}

bool IsBinaryData(std::string_view data, std::string_view contentType) {
    if (contentType.find("image/") != std::string_view::npos ||
        contentType.find("audio/") != std::string_view::npos ||
        contentType.find("video/") != std::string_view::npos ||
        contentType.find("font/") != std::string_view::npos ||
        contentType.find("application/octet-stream") != std::string_view::npos ||
        contentType.find("application/zip") != std::string_view::npos ||
        contentType.find("application/gzip") != std::string_view::npos ||
        contentType.find("application/pdf") != std::string_view::npos) {
        return true;
    }

    std::size_t sample = std::min<std::size_t>(data.size(), 512);
    if (sample == 0) return false;
    std::size_t nonPrintable = 0;
    for (std::size_t i = 0; i < sample; ++i) {
        unsigned char c = static_cast<unsigned char>(data[i]);
        if (c == 0) return true; // Null byte indicates binary
        if (c < 9 || (c > 13 && c < 32)) {
            nonPrintable++;
        }
    }
    return (nonPrintable * 100 / sample) > 8;
}

std::string FormatHexDump(std::string_view data, std::size_t maxBytes) {
    std::ostringstream oss;
    std::size_t limit = std::min(data.size(), maxBytes);
    for (std::size_t i = 0; i < limit; i += 16) {
        char offsetBuf[32];
        std::snprintf(offsetBuf, sizeof(offsetBuf), "%08zx: ", i);
        oss << offsetBuf;

        for (std::size_t j = 0; j < 16; ++j) {
            if (i + j < limit) {
                char byteBuf[4];
                std::snprintf(byteBuf, sizeof(byteBuf), "%02x ", static_cast<unsigned char>(data[i + j]));
                oss << byteBuf;
            } else {
                oss << "   ";
            }
            if (j == 7) oss << " ";
        }
        oss << " |";
        for (std::size_t j = 0; j < 16 && i + j < limit; ++j) {
            unsigned char c = static_cast<unsigned char>(data[i + j]);
            oss << (c >= 32 && c < 127 ? static_cast<char>(c) : '.');
        }
        oss << "|\n";
    }
    if (data.size() > limit) {
        oss << "[ ... " << (data.size() - limit) << " additional binary bytes omitted ... ]\n";
    }
    return oss.str();
}

Elements FormatBodyLines(const std::string& body, const std::string& contentType) {
    Elements lines;
    if (body.empty()) {
        lines.push_back(text("(Empty body)") | dim);
        return lines;
    }
    if (IsBinaryData(body, contentType)) {
        lines.push_back(
            text("[ Binary Payload: " + std::to_string(body.size()) + " bytes | Content-Type: " + 
                 (contentType.empty() ? "unknown" : contentType) + " ]") | color(Color::YellowLight)
        );
        lines.push_back(text(""));
        
        std::string hex = FormatHexDump(body, 1024);
        std::istringstream stream(hex);
        std::string line;
        while (std::getline(stream, line)) {
            lines.push_back(text(line) | color(Color::GrayLight));
        }
        return lines;
    }

    // Text body: split line by line
    constexpr std::size_t MAX_LINES = 1000;
    std::istringstream stream(body);
    std::string line;
    std::size_t lineCount = 0;
    while (std::getline(stream, line) && lineCount < MAX_LINES) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(text(SanitizeText(line, 512)) | color(Color::White));
        lineCount++;
    }

    if (stream.good()) {
        lines.push_back(text("[ ... Remaining lines truncated for TUI display ... ]") | dim);
    }

    return lines;
}

Element FormatBodyElement(const std::string& body, const std::string& contentType) {
    return vbox(FormatBodyLines(body, contentType));
}

Elements FormatHttpRequestLines(const HttpRequest& req, bool isHttps, const std::string& host) {
    Elements elements;
    // Method line: GET /path HTTP/1.1
    elements.push_back(hbox({
        text(req.method + " ") | bold | color(Color::Cyan),
        text(SanitizeText(req.url, 256) + " ") | bold | color(Color::White) | xflex_shrink,
        text(req.version) | dim,
    }));
    elements.push_back(separatorLight());

    // Host & Scheme info
    elements.push_back(hbox({
        text("Host: ") | bold | color(Color::BlueLight),
        text(host + (isHttps ? " (HTTPS)" : " (HTTP)")) | color(Color::White),
    }));

    // Headers
    for (const auto& [k, v] : req.headers) {
        if (k == "Host") continue;
        elements.push_back(hbox({
            text(SanitizeText(k, 64) + ": ") | bold | color(Color::BlueLight),
            text(SanitizeText(v, 256)) | color(Color::White),
        }));
    }

    // Body
    if (!req.body.empty()) {
        elements.push_back(separatorLight());
        elements.push_back(text("--- Request Body (" + std::to_string(req.body.size()) + " bytes) ---") | dim);
        auto bodyLines = FormatBodyLines(req.body, req.header("Content-Type"));
        for (auto& bl : bodyLines) {
            elements.push_back(std::move(bl));
        }
    }

    return elements;
}

Elements FormatHttpResponseLines(const HttpResponse& res) {
    Elements elements;
    // Status line: HTTP/1.1 200 OK
    Color statusColor = (res.statusCode < 300) ? Color::Green :
                        (res.statusCode < 400) ? Color::Yellow : Color::Red;

    elements.push_back(hbox({
        text(res.version + " ") | dim,
        text(std::to_string(res.statusCode) + " " + SanitizeText(res.statusText, 64)) | bold | color(statusColor),
    }));
    elements.push_back(separatorLight());

    // Headers
    for (const auto& [k, v] : res.headers) {
        elements.push_back(hbox({
            text(SanitizeText(k, 64) + ": ") | bold | color(Color::BlueLight),
            text(SanitizeText(v, 256)) | color(Color::White),
        }));
    }

    // Body
    if (!res.body.empty()) {
        elements.push_back(separatorLight());
        elements.push_back(text("--- Response Body (" + std::to_string(res.body.size()) + " bytes) ---") | dim);
        auto bodyLines = FormatBodyLines(res.body, res.header("Content-Type"));
        for (auto& bl : bodyLines) {
            elements.push_back(std::move(bl));
        }
    }

    return elements;
}

Element FormatHttpRequest(const HttpRequest& req, bool isHttps, const std::string& host) {
    return vbox(FormatHttpRequestLines(req, isHttps, host));
}

Element FormatHttpResponse(const HttpResponse& res) {
    return vbox(FormatHttpResponseLines(res));
}

std::string FormatHttpRequestRaw(const HttpRequest& req, bool isHttps, const std::string& host) {
    std::ostringstream oss;
    oss << req.method << " " << req.url << " " << req.version << "\r\n";
    oss << "Host: " << host << (isHttps ? " (HTTPS)" : "") << "\r\n";
    for (const auto& [k, v] : req.headers) {
        if (k == "Host") continue;
        oss << k << ": " << v << "\r\n";
    }
    oss << "\r\n" << req.body;
    return oss.str();
}

std::string FormatHttpResponseRaw(const HttpResponse& res) {
    std::ostringstream oss;
    oss << res.version << " " << res.statusCode << " " << res.statusText << "\r\n";
    for (const auto& [k, v] : res.headers) {
        oss << k << ": " << v << "\r\n";
    }
    oss << "\r\n" << res.body;
    return oss.str();
}

} // namespace BurpTUI
