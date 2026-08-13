#pragma once
#include <string>
#include <memory>
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

namespace BurpTUI {

/// Represents a complete intercepted HTTP transaction
struct HttpTransaction {
    int id = 0;          // Transaction ID
    std::string host;
    int port = 0;
    bool is_https = false;
    
    std::shared_ptr<HttpRequest> request;
    std::shared_ptr<HttpResponse> response;
};

} // namespace BurpTUI
