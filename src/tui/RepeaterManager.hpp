#pragma once
#include <string>
#include <cstdint>
#include <functional>
#include <mutex>
#include <memory>

namespace BurpTUI {

class RepeaterManager {
public:
    static RepeaterManager& instance();

    // Data getters/setters
    void setTarget(std::string host, std::string port, bool isHttps);
    void setRequest(std::string host, std::string port, bool isHttps, std::string rawRequest);
    void setRawRequest(std::string raw);
    void setRawResponse(std::string raw);

    std::string getHost() const;
    std::string getPort() const;
    bool isHttps() const;
    std::string getRawRequest() const;
    std::string getRawResponse() const;
    std::string getStatusText() const;
    bool isSending() const;

    // Execute the request over network asynchronously
    void executeRequest();

    // UI notifications and tab switching
    void setNotifyCallback(std::function<void()> callback);
    void setSwitchTabCallback(std::function<void(int)> callback);
    void switchToRepeaterTab();

private:
    RepeaterManager();
    ~RepeaterManager() = default;
    RepeaterManager(const RepeaterManager&) = delete;
    RepeaterManager& operator=(const RepeaterManager&) = delete;

    mutable std::mutex mutex_;
    std::string host_{"httpbin.org"};
    std::string port_{"443"};
    bool isHttps_{true};
    std::string rawRequest_{
        "GET /get HTTP/1.1\r\n"
        "Host: httpbin.org\r\n"
        "User-Agent: BurpTUI/0.1\r\n"
        "Accept: */*\r\n\r\n"
    };
    std::string rawResponse_{"(Send request to see real response here)"};
    std::string statusText_{"Ready"};
    bool isSending_{false};

    std::function<void()> notifyCallback_;
    std::function<void(int)> switchTabCallback_;
};

} // namespace BurpTUI
