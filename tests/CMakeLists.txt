find_package(GTest CONFIG REQUIRED)

# ── Helper library (everything except main.cpp) ──────────
# Reuse the same source list but exclude the entry point
set(LIB_SOURCES
    # App coordinator
    ${CMAKE_SOURCE_DIR}/src/app/App.cpp

    # Proxy engine
    ${CMAKE_SOURCE_DIR}/src/proxy/ProxyServer.cpp
    ${CMAKE_SOURCE_DIR}/src/proxy/Session.cpp
    ${CMAKE_SOURCE_DIR}/src/proxy/MitmSession.cpp
    ${CMAKE_SOURCE_DIR}/src/proxy/CertGenerator.cpp
    ${CMAKE_SOURCE_DIR}/src/proxy/CertCache.cpp

    # HTTP parsing
    ${CMAKE_SOURCE_DIR}/src/http/HttpParser.cpp
    ${CMAKE_SOURCE_DIR}/src/http/HttpRequest.cpp
    ${CMAKE_SOURCE_DIR}/src/http/HttpResponse.cpp

    # Storage layer
    ${CMAKE_SOURCE_DIR}/src/storage/SqliteStore.cpp
    ${CMAKE_SOURCE_DIR}/src/storage/MemoryStore.cpp

    # TUI interface
    ${CMAKE_SOURCE_DIR}/src/tui/TuiApp.cpp
    ${CMAKE_SOURCE_DIR}/src/tui/ProxyTab.cpp
    ${CMAKE_SOURCE_DIR}/src/tui/HistoryTab.cpp
    ${CMAKE_SOURCE_DIR}/src/tui/RepeaterTab.cpp
    ${CMAKE_SOURCE_DIR}/src/tui/DecoderTab.cpp
    ${CMAKE_SOURCE_DIR}/src/tui/Widgets.cpp

    # Utilities
    ${CMAKE_SOURCE_DIR}/src/util/Logger.cpp
    ${CMAKE_SOURCE_DIR}/src/util/Encoding.cpp
)

add_library(burptui_lib STATIC ${LIB_SOURCES})
target_include_directories(burptui_lib PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_compile_options(burptui_lib PRIVATE -Wall -Wextra)
target_link_libraries(burptui_lib PUBLIC
    Boost::system
    OpenSSL::SSL
    OpenSSL::Crypto
    ftxui::screen
    ftxui::dom
    ftxui::component
    SQLite::SQLite3
)
if(unofficial-llhttp_FOUND)
    target_link_libraries(burptui_lib PUBLIC unofficial::llhttp::llhttp)
elseif(llhttp_FOUND)
    if(TARGET llhttp::llhttp_static)
        target_link_libraries(burptui_lib PUBLIC llhttp::llhttp_static)
    else()
        target_link_libraries(burptui_lib PUBLIC llhttp::llhttp)
    endif()
else()
    target_link_libraries(burptui_lib PUBLIC PkgConfig::LLHTTP)
endif()


# ── Individual test executables ──────────────────────────
function(add_burptui_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE burptui_lib GTest::gtest GTest::gtest_main)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_burptui_test(test_encoding      test_encoding.cpp)
add_burptui_test(test_http_parser   test_http_parser.cpp)
add_burptui_test(test_storage       test_storage.cpp)
add_burptui_test(test_cert_generator test_cert_generator.cpp)
add_burptui_test(test_event_queue   test_event_queue.cpp)
