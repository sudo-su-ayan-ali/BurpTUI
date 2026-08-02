cmake_minimum_required(VERSION 3.20)
project(burptui CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Dependencies ─────────────────────────────────────────
find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)
find_package(ftxui CONFIG REQUIRED)
find_package(SQLite3 REQUIRED)
# llhttp is typically found via pkg-config or vcpkg
find_package(unofficial-llhttp CONFIG QUIET)
if(NOT unofficial-llhttp_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LLHTTP REQUIRED IMPORTED_TARGET llhttp)
endif()

# ── Main executable ──────────────────────────────────────
add_executable(burptui
    src/main.cpp

    # App coordinator
    src/app/App.cpp

    # Proxy engine
    src/proxy/ProxyServer.cpp
    src/proxy/Session.cpp
    src/proxy/MitmSession.cpp
    src/proxy/CertGenerator.cpp
    src/proxy/CertCache.cpp

    # HTTP parsing
    src/http/HttpParser.cpp
    src/http/HttpRequest.cpp
    src/http/HttpResponse.cpp

    # Storage layer
    src/storage/SqliteStore.cpp
    src/storage/MemoryStore.cpp

    # TUI interface
    src/tui/TuiApp.cpp
    src/tui/ProxyTab.cpp
    src/tui/HistoryTab.cpp
    src/tui/RepeaterTab.cpp
    src/tui/DecoderTab.cpp

    # Utilities
    src/util/Logger.cpp
    src/util/Encoding.cpp
)

target_include_directories(burptui PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(burptui PRIVATE
    Boost::system
    OpenSSL::SSL
    OpenSSL::Crypto
    ftxui::screen
    ftxui::dom
    ftxui::component
    SQLite::SQLite3
)

# Link llhttp depending on how it was found
if(unofficial-llhttp_FOUND)
    target_link_libraries(burptui PRIVATE unofficial::llhttp::llhttp)
else()
    target_link_libraries(burptui PRIVATE PkgConfig::LLHTTP)
endif()

# ── Tests ────────────────────────────────────────────────
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
