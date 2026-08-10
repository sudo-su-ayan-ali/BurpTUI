#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace BurpTUI {

/// Lightweight HTTP response model.
struct HttpResponse {
    std::string              version;
    int                      statusCode = 0;
    std::string              statusText;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string              body;

    [[nodiscard]] std::string header(std::string_view name) const;
    [[nodiscard]] std::string serialize() const;
};

} // namespace BurpTUI
