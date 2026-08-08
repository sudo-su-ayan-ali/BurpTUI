#pragma once
#include <memory>
#include "Store.hpp"
#include <string>

namespace BurpTUI {

class SqliteStore final : public Store {
public:
    explicit SqliteStore(const std::string& dbPath);
    ~SqliteStore() override;

    void save(const ProxyEntry& entry) override;
    std::vector<ProxyEntry> list() const override;
    std::optional<ProxyEntry> get(std::uint64_t id) const override;
    void clear() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace BurpTUI
