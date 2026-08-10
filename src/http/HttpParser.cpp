#include "http/HttpParser.hpp"
#include "util/Logger.hpp"
#include <llhttp.h>
#include <string>
#include <vector>

namespace BurpTUI {

struct HttpParser::Impl {
    struct RequestParser {
        llhttp_t parser;
        llhttp_settings_t settings;
        HttpRequest request;
        std::string currentHeaderField;
        std::string currentHeaderValue;
        bool lastWasField = false;
        bool messageComplete = false;

        RequestParser() {
            llhttp_settings_init(&settings);
            settings.on_url = on_url;
            settings.on_method = on_method;
            settings.on_header_field = on_header_field;
            settings.on_header_value = on_header_value;
            settings.on_body = on_body;
            settings.on_message_complete = on_message_complete;
            llhttp_init(&parser, HTTP_REQUEST, &settings);
            parser.data = this;
        }

        void reset() {
            request = HttpRequest{};
            currentHeaderField.clear();
            currentHeaderValue.clear();
            lastWasField = false;
            messageComplete = false;
            llhttp_init(&parser, HTTP_REQUEST, &settings);
            parser.data = this;
        }

        void commitHeader() {
            if (!currentHeaderField.empty()) {
                Logger::instance().debug("Parsed Header: " + currentHeaderField + ": " + currentHeaderValue);
                request.headers.push_back({currentHeaderField, currentHeaderValue});
                currentHeaderField.clear();
                currentHeaderValue.clear();
            }
        }

        static int on_method(llhttp_t* /*p*/, const char* at, size_t length) {
            std::string_view method(at, length);
            Logger::instance().debug("Parsed Method chunk: " + std::string(method));
            return 0;
        }

        static int on_url(llhttp_t* p, const char* at, size_t length) {
            auto* self = static_cast<RequestParser*>(p->data);
            std::string_view url(at, length);
            Logger::instance().debug("Parsed URL chunk: " + std::string(url));
            self->request.url.append(at, length);
            return 0;
        }

        static int on_header_field(llhttp_t* p, const char* at, size_t length) {
            auto* self = static_cast<RequestParser*>(p->data);
            if (!self->lastWasField) {
                self->commitHeader();
            }
            self->currentHeaderField.append(at, length);
            self->lastWasField = true;
            return 0;
        }

        static int on_header_value(llhttp_t* p, const char* at, size_t length) {
            auto* self = static_cast<RequestParser*>(p->data);
            self->currentHeaderValue.append(at, length);
            self->lastWasField = false;
            return 0;
        }

        static int on_body(llhttp_t* p, const char* at, size_t length) {
            auto* self = static_cast<RequestParser*>(p->data);
            self->request.body.append(at, length);
            return 0;
        }

        static int on_message_complete(llhttp_t* p) {
            auto* self = static_cast<RequestParser*>(p->data);
            self->commitHeader();
            self->request.method = llhttp_method_name(static_cast<llhttp_method>(p->method));
            self->request.version = "HTTP/" + std::to_string(p->http_major) + "." + std::to_string(p->http_minor);
            self->messageComplete = true;
            return 0;
        }
    };

    struct ResponseParser {
        llhttp_t parser;
        llhttp_settings_t settings;
        HttpResponse response;
        std::string currentHeaderField;
        std::string currentHeaderValue;
        bool lastWasField = false;
        bool messageComplete = false;

        ResponseParser() {
            llhttp_settings_init(&settings);
            settings.on_status = on_status;
            settings.on_header_field = on_header_field;
            settings.on_header_value = on_header_value;
            settings.on_body = on_body;
            settings.on_message_complete = on_message_complete;
            llhttp_init(&parser, HTTP_RESPONSE, &settings);
            parser.data = this;
        }

        void reset() {
            response = HttpResponse{};
            currentHeaderField.clear();
            currentHeaderValue.clear();
            lastWasField = false;
            messageComplete = false;
            llhttp_init(&parser, HTTP_RESPONSE, &settings);
            parser.data = this;
        }

        void commitHeader() {
            if (!currentHeaderField.empty()) {
                Logger::instance().debug("Parsed Header: " + currentHeaderField + ": " + currentHeaderValue);
                response.headers.push_back({currentHeaderField, currentHeaderValue});
                currentHeaderField.clear();
                currentHeaderValue.clear();
            }
        }

        static int on_status(llhttp_t* p, const char* at, size_t length) {
            auto* self = static_cast<ResponseParser*>(p->data);
            std::string_view status(at, length);
            Logger::instance().debug("Parsed Status chunk: " + std::string(status));
            self->response.statusText.append(at, length);
            return 0;
        }

        static int on_header_field(llhttp_t* p, const char* at, size_t length) {
            auto* self = static_cast<ResponseParser*>(p->data);
            if (!self->lastWasField) {
                self->commitHeader();
            }
            self->currentHeaderField.append(at, length);
            self->lastWasField = true;
            return 0;
        }

        static int on_header_value(llhttp_t* p, const char* at, size_t length) {
            auto* self = static_cast<ResponseParser*>(p->data);
            self->currentHeaderValue.append(at, length);
            self->lastWasField = false;
            return 0;
        }

        static int on_body(llhttp_t* p, const char* at, size_t length) {
            auto* self = static_cast<ResponseParser*>(p->data);
            self->response.body.append(at, length);
            return 0;
        }

        static int on_message_complete(llhttp_t* p) {
            auto* self = static_cast<ResponseParser*>(p->data);
            self->commitHeader();
            self->response.statusCode = p->status_code;
            self->response.version = "HTTP/" + std::to_string(p->http_major) + "." + std::to_string(p->http_minor);
            self->messageComplete = true;
            return 0;
        }
    };

    RequestParser requestParser;
    ResponseParser responseParser;
};

HttpParser::HttpParser() : impl_(std::make_unique<Impl>()) {}
HttpParser::~HttpParser() = default;

void HttpParser::reset() {
    impl_->requestParser.reset();
    impl_->responseParser.reset();
}

bool HttpParser::feedRequest(std::string_view data) {
    if (impl_->requestParser.messageComplete) return true;
    llhttp_errno err = llhttp_execute(&impl_->requestParser.parser, data.data(), data.size());
    if (err != HPE_OK && err != HPE_PAUSED_UPGRADE) {
        return false;
    }
    return impl_->requestParser.messageComplete;
}

bool HttpParser::feedResponse(std::string_view data) {
    if (impl_->responseParser.messageComplete) return true;
    llhttp_errno err = llhttp_execute(&impl_->responseParser.parser, data.data(), data.size());
    if (err != HPE_OK && err != HPE_PAUSED_UPGRADE) {
        return false;
    }
    return impl_->responseParser.messageComplete;
}

std::optional<HttpRequest> HttpParser::takeRequest() {
    if (impl_->requestParser.messageComplete) {
        HttpRequest req = std::move(impl_->requestParser.request);
        impl_->requestParser.reset();
        return req;
    }
    return std::nullopt;
}

std::optional<HttpResponse> HttpParser::takeResponse() {
    if (impl_->responseParser.messageComplete) {
        HttpResponse res = std::move(impl_->responseParser.response);
        impl_->responseParser.reset();
        return res;
    }
    return std::nullopt;
}

} // namespace BurpTUI
