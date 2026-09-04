#include "util/Clipboard.hpp"
#include "util/Encoding.hpp"
#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>

namespace BurpTUI {

namespace {

void dispatchSystemClipboard(std::string text) {
    int fds[2];
    if (pipe(fds) != 0) return;

    pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return;
    }

    if (pid == 0) {
        // First child: fork grandchild and exit so parent doesn't block
        pid_t pid2 = fork();
        if (pid2 > 0) {
            _exit(0);
        }
        if (pid2 < 0) {
            _exit(1);
        }

        // Grandchild: detached session
        setsid();
        close(fds[1]);
        dup2(fds[0], STDIN_FILENO);
        close(fds[0]);

        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }

        const char* disp = getenv("DISPLAY");
        if (disp && disp[0] != '\0') {
            execlp("xclip", "xclip", "-selection", "clipboard", nullptr);
            execlp("xsel", "xsel", "--clipboard", "--input", nullptr);
        } else {
            execlp("wl-copy", "wl-copy", nullptr);
        }
        _exit(1);
    }

    // Parent
    close(fds[0]);
    if (!text.empty()) {
        ssize_t written = 0;
        ssize_t total = static_cast<ssize_t>(text.size());
        while (written < total) {
            ssize_t n = write(fds[1], text.data() + written, total - written);
            if (n <= 0) break;
            written += n;
        }
    }
    close(fds[1]);
    int status = 0;
    waitpid(pid, &status, 0);
}

} // namespace

bool Clipboard::copy(std::string_view text) {
    if (text.empty()) return false;

    // 1. Always emit OSC 52 sequence (instant, works in terminal, SSH, tmux, Windows Terminal)
    try {
        std::string b64 = Encoding::base64Encode(text);
        std::cout << "\033]52;c;" << b64 << "\007" << std::flush;
    } catch (...) {}

    // 2. Dispatch system clipboard via detached double-fork (non-blocking)
    dispatchSystemClipboard(std::string(text));

    return true;
}

} // namespace BurpTUI
