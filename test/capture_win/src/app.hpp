/**
 * @file app.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-02-22
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#include <string>
#include <vector>
#include <any>
#include <ilias/ilias.hpp>
#include <ilias/net/tcp.hpp>
#include <ilias/fs/console.hpp>

#include "command_parser.hpp"
#include "mk_capture.hpp"
#include "mk_sender.hpp"

class App {
public:
    App();
    ~App();

    static auto app_name() -> const char *;
    static auto app_version() -> const char *;

    auto exec(int argc = 0, const char *const *argv = nullptr) -> ILIAS_NAMESPACE::Task<void>;
    auto stop() -> void;

    template <typename T>
    auto set_option(std::string_view option, const T &value);
    template <typename T>
    auto get_option(std::string_view option) -> ILIAS_NAMESPACE::Result<T>;
    // server
    /**
     * @brief start server
     * 如果需要ilias_go参数列表必须复制。
     * @param endpoint
     * @return Task<void>
     */
    auto start_server(ILIAS_NAMESPACE::IPEndpoint endpoint) -> ILIAS_NAMESPACE::Task<void>;
    auto stop_server() -> void;
    auto start_capture() -> ILIAS_NAMESPACE::Task<void>;
    auto stop_capture() -> void;

    // client
    /**
     * @brief connect to server
     * 如果需要ilias_go参数列表必须复制。
     * @param endpoint
     * @return Task<void>
     */
    auto connect_to(ILIAS_NAMESPACE::IPEndpoint endpoint) -> ILIAS_NAMESPACE::Task<void>;
    auto disconnect() -> void;

    // commands
    auto start_console() -> ILIAS_NAMESPACE::Task<void>;
    auto stop_console() -> void;

private:
    // for server
    auto _accept_client(ILIAS_NAMESPACE::TcpClient client) -> ILIAS_NAMESPACE::Task<void>;

private:
    bool                  _isClienting        = false;
    bool                  _isServering        = false;
    bool                  _isCapturing        = false;
    bool                  _isRuning           = false;
    bool                  _isConsoleListening = false;
    NekoProto::StreamFlag _streamflags        = NekoProto::StreamFlag::None;

    ILIAS_NAMESPACE::TcpListener                    _server;
    CommandParser                                   _commandParser;
    std::unordered_map<std::string, std::any>       _optionsMap;
    std::unique_ptr<MKCapture>                      _listener;
    std::unique_ptr<MKSender>                       _eventSender;
    std::unique_ptr<NekoProto::ProtoStreamClient<>> _protoStreamClient;
    NekoProto::ProtoFactory                         _protofactory;
};

template <typename T>
auto App::set_option(std::string_view option, const T &value)
{
    _optionsMap[std::string(option)] = value;
}

template <typename T>
auto App::get_option(std::string_view option) -> ILIAS_NAMESPACE::Result<T>
{
    auto it = _optionsMap.find(std::string(option));
    if (it == _optionsMap.end()) {
        return ILIAS_NAMESPACE::Unexpected<ILIAS_NAMESPACE::Error>(ILIAS_NAMESPACE::Error::Unknown);
    }
    return std::any_cast<T>(it->second);
}