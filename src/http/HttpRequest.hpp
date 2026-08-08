#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace BurpTUI {

/// Lightweight HTTP request model.
struct HttpRequest {
    std::string              method;
    std::string              url;
    std::string              version;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string              body;

    [[nodiscard]] std::string header(std::string_view name) const;
    [[nodiscard]] std::string serialize() const;
};

} // namespace BurpTUI
