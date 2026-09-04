#include "app/App.hpp"
#include <string_view>
#include <iostream>

int main(int argc, char* argv[]) {
    BurpTUI::Config cfg;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--mouse" || arg == "-m") {
            cfg.enableMouse = true;
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "BurpTUI - Terminal-based HTTP/HTTPS Proxy & Interceptor\n\n"
                      << "Usage: burptui [options]\n\n"
                      << "Options:\n"
                      << "  --mouse, -m    Enable TUI mouse reporting (Note: suppresses native terminal text selection)\n"
                      << "  --port <port>  Set proxy port (default: 8080)\n"
                      << "  --host <host>  Set proxy listen host (default: 127.0.0.1)\n"
                      << "  -h, --help     Show this help message\n\n"
                      << "Default: Native text selection & right-click are enabled out of the box!\n";
            return 0;
        } else if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
            cfg.listenPort = static_cast<std::uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--host" && i + 1 < argc) {
            cfg.listenHost = argv[++i];
        }
    }
    BurpTUI::App app(cfg);
    return app.run();
}
