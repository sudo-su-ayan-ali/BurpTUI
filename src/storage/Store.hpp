#pragma once
#include <string>
#include <string_view>
#include <cstdint>
#include <vector>
#include <optional>

namespace BurpTUI {

/// Captured proxy traffic entry.
struct ProxyEntry {
    std::uint64_t id = 0;
    std::string   method;
    std::string   host;
    std::string   url;
    int           statusCode  = 0;
    std::size_t   responseLen = 0;
    std::string   timestamp;

    // Raw data (lazy-loaded)
    std::string   rawRequest;
    std::string   rawResponse;
};

/// Abstract persistence interface.
class Store {
public:
    virtual ~Store() = default;

    virtual void   save(const ProxyEntry& entry) = 0;
    virtual std::vector<ProxyEntry> list() const = 0;
    virtual std::optional<ProxyEntry> get(std::uint64_t id) const = 0;
    virtual void   clear() = 0;
};

} // namespace BurpTUI
