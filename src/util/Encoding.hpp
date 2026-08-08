#pragma once
#include <string>
#include <vector>

namespace BurpTUI {
namespace Encoding {

std::string base64Encode(std::string_view input);
std::string base64Decode(std::string_view input);

std::string urlEncode(std::string_view input);
std::string urlDecode(std::string_view input);

std::string hexEncode(std::string_view input);
std::string hexDecode(std::string_view input);

} // namespace Encoding
} // namespace BurpTUI
