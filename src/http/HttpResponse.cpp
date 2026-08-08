#include "http/HttpResponse.hpp"
#include <sstream>

namespace BurpTUI {

std::string HttpResponse::serialize() const {
    std::ostringstream oss;
    oss << version << " " << statusCode << " " << statusText << "\r\n";
    for (const auto& [k, v] : headers)
        oss << k << ": " << v << "\r\n";
    oss << "\r\n" << body;
    return oss.str();
}

} // namespace BurpTUI
