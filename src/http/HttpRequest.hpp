#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace BurpTUI {

/// Lightweight HTTP request model.
struct HttpRequest {
    std::string              method;
    std::string              url;
    std::string              version{"HTTP/1.1"};
    std::vector<std::pair<std::string, std::string>> headers;
    std::string              body;
    std::string              rawOverride;

    [[nodiscard]] std::string header(std::string_view name) const;
    [[nodiscard]] std::string serialize() const;
};

/// Parse raw HTTP request text into an HttpRequest structure.
HttpRequest ParseRawHttpRequest(std::string_view raw);

} // namespace BurpTUI
