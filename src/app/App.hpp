#pragma once
#include "Config.hpp"

namespace BurpTUI {

/// Top-level application coordinator.
class App {
public:
    explicit App(Config cfg = {});
    ~App();

    /// Blocking entry point; returns the process exit code.
    int run();

private:
    Config cfg_;
};

} // namespace BurpTUI
