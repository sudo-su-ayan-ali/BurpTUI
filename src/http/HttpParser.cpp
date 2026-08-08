#include "http/HttpParser.hpp"

namespace BurpTUI {

// Pimpl — full llhttp integration deferred to Phase 2.
struct HttpParser::Impl {};

HttpParser::HttpParser()  : impl_(new Impl{}) {}
HttpParser::~HttpParser() { delete impl_; }

bool HttpParser::feedRequest(std::string_view /*data*/)  { return false; }
bool HttpParser::feedResponse(std::string_view /*data*/) { return false; }

std::optional<HttpRequest>  HttpParser::takeRequest()  { return std::nullopt; }
std::optional<HttpResponse> HttpParser::takeResponse() { return std::nullopt; }

} // namespace BurpTUI
