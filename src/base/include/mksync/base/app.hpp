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
#include <any>
#include <ilias/ilias.hpp>
#include <ilias/net/tcp.hpp>
#include <ilias/fs/console.hpp>

#include "mksync/base/command_parser.hpp"
#include "mksync/base/mk_capture.hpp"
#include "mksync/base/mk_sender.hpp"
#include "mksync/base/base_library.h"

namespace mks::base
{
    class MKS_BASE_API App {
    public:
        App();
        ~App();

        static auto app_name() -> const char *;
        static auto app_version() -> const char *;

        // main loop
        auto exec(int argc = 0, const char *const *argv = nullptr) -> ilias::Task<void>;
        auto stop() -> void;
        auto stop(const CommandParser::ArgsType &args, const CommandParser::OptionsType &options)
            -> std::string;

        template <typename T>
        auto set_option(std::string_view option, const T &value);
        template <typename T>
        auto get_option(std::string_view option) -> ilias::Result<T>;

        // server
        /**
         * @brief start server
         * 如果需要ilias_go参数列表必须复制。
         * @param endpoint
         * @return Task<void>
         */
        auto start_server(ilias::IPEndpoint endpoint) -> ilias::Task<void>;
        auto stop_server() -> void;
        auto start_capture() -> ilias::Task<void>;
        auto stop_capture() -> void;
        auto start_server(const CommandParser::ArgsType    &args,
                          const CommandParser::OptionsType &options) -> std::string;
        auto stop_server(const CommandParser::ArgsType    &args,
                         const CommandParser::OptionsType &options) -> std::string;
        auto start_capture(const CommandParser::ArgsType    &args,
                           const CommandParser::OptionsType &options) -> std::string;
        auto stop_capture(const CommandParser::ArgsType    &args,
                          const CommandParser::OptionsType &options) -> std::string;

        // client
        /**
         * @brief connect to server
         * 如果需要ilias_go参数列表必须复制。
         * @param endpoint
         * @return Task<void>
         */
        auto connect_to(ilias::IPEndpoint endpoint) -> ilias::Task<void>;
        auto disconnect() -> void;
        auto connect_to(const CommandParser::ArgsType    &args,
                        const CommandParser::OptionsType &options) -> std::string;
        auto disconnect(const CommandParser::ArgsType    &args,
                        const CommandParser::OptionsType &options) -> std::string;

        // communication
        auto set_communication_options(const CommandParser::ArgsType    &args,
                                       const CommandParser::OptionsType &options) -> std::string;

        // commands
        auto start_console() -> ilias::Task<void>;
        auto stop_console() -> void;
        auto command_installer(std::string_view module)
            -> std::function<bool(CommandParser::CommandsData &&)>;

    private:
        // for server
        auto _accept_client(ilias::TcpClient client) -> ilias::Task<void>;

    private:
        bool                  _isClienting        = false;
        bool                  _isServering        = false;
        bool                  _isCapturing        = false;
        bool                  _isRuning           = false;
        bool                  _isConsoleListening = false;
        NekoProto::StreamFlag _streamflags        = NekoProto::StreamFlag::None;

        ilias::TcpListener                              _server;
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
    auto App::get_option(std::string_view option) -> ilias::Result<T>
    {
        auto it = _optionsMap.find(std::string(option));
        if (it == _optionsMap.end()) {
            return ilias::Unexpected<ilias::Error>(ilias::Error::Unknown);
        }
        try {
            return std::any_cast<T>(it->second);
        }
        catch (std::bad_any_cast &e) {
            return ilias::Unexpected<ilias::Error>(ilias::Error::Unknown);
        }
    }
} // namespace mks::base
