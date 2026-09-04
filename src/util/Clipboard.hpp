#pragma once
#include <string_view>

namespace BurpTUI {

class Clipboard {
public:
    /// Copy text to system clipboard using OSC 52, xclip, and wl-copy.
    static bool copy(std::string_view text);
};

} // namespace BurpTUI
