#pragma once
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include <optional>
#include <string_view>

namespace BurpTUI {

/// Minimal incremental HTTP parser (wraps llhttp).
class HttpParser {
public:
    HttpParser();
    ~HttpParser();

    /// Feed raw bytes; returns true when a complete message is ready.
    bool feedRequest(std::string_view data);
    bool feedResponse(std::string_view data);

    std::optional<HttpRequest>  takeRequest();
    std::optional<HttpResponse> takeResponse();

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace BurpTUI
