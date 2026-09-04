#pragma once
#include <ftxui/dom/elements.hpp>
#include <string>
#include <string_view>
#include <vector>
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"

namespace BurpTUI {

/// Clean text: strip \r and replace dangerous ANSI control characters that break terminal layout.
std::string SanitizeText(std::string_view input, std::size_t maxLen = 4096);

/// Detect binary data (e.g. gzip compressed, images, executables, fonts).
bool IsBinaryData(std::string_view data, std::string_view contentType = "");

/// Format binary payload as a readable hex dump preview.
std::string FormatHexDump(std::string_view data, std::size_t maxBytes = 256);

/// Format body into lines of FTXUI Elements.
ftxui::Elements FormatBodyLines(const std::string& body, const std::string& contentType = "");

/// Format body into FTXUI Element (hex dump for binary, line-by-line for text).
ftxui::Element FormatBodyElement(const std::string& body, const std::string& contentType = "");

/// Format HTTP request into a vector of line elements for scrollable viewing.
ftxui::Elements FormatHttpRequestLines(const HttpRequest& req, bool isHttps, const std::string& host);

/// Format HTTP response into a vector of line elements for scrollable viewing.
ftxui::Elements FormatHttpResponseLines(const HttpResponse& res);

/// Format HTTP request into a single Element.
ftxui::Element FormatHttpRequest(const HttpRequest& req, bool isHttps, const std::string& host);

/// Format HTTP response into a single Element.
ftxui::Element FormatHttpResponse(const HttpResponse& res);

/// Format HTTP request as raw plain text suitable for clipboard copy.
std::string FormatHttpRequestRaw(const HttpRequest& req, bool isHttps, const std::string& host);

/// Format HTTP response as raw plain text suitable for clipboard copy.
std::string FormatHttpResponseRaw(const HttpResponse& res);

} // namespace BurpTUI
