#include "http/HttpRequest.hpp"
#include <sstream>

namespace BurpTUI {

std::string HttpRequest::header(std::string_view name) const {
    for (const auto& [k, v] : headers)
        if (k == name) return v;
    return {};
}

std::string HttpRequest::serialize() const {
    std::ostringstream oss;
    oss << method << " " << url << " " << version << "\r\n";
    for (const auto& [k, v] : headers)
        oss << k << ": " << v << "\r\n";
    oss << "\r\n" << body;
    return oss.str();
}

} // namespace BurpTUI
