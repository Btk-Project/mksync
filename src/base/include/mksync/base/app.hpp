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
#include <set>
#include <ilias/ilias.hpp>
#include <ilias/net/tcp.hpp>
#include <ilias/fs/console.hpp>

#include "mksync/base/command_parser.hpp"
#include "mksync/base/mk_capture.hpp"
#include "mksync/base/mk_sender.hpp"
#include "mksync/base/base_library.h"
#include "mksync/base/nodebase.hpp"

namespace mks::base
{
    class MKS_BASE_API App {
        enum NodeStatus
        {
            eNodeStatusRunning = 1,
            eNodeStatusStopped = 2,
        };
        struct NodeData {
            std::unique_ptr<NodeBase, void (*)(NodeBase *)> node;
            NodeStatus                                      status;
        };

    public:
        App(::ilias::IoContext *ctx);
        ~App();

        static auto app_name() -> const char *;
        static auto app_version() -> const char *;
        auto        get_io_context() const -> ::ilias::IoContext *;

        // main loop
        auto exec(int argc = 0, const char *const *argv = nullptr) -> ilias::Task<void>;
        auto distribution(NekoProto::IProto &&proto) -> void;
        auto install_node(std::unique_ptr<NodeBase, void (*)(NodeBase *)> &&node) -> void;
        auto start_node(std::string_view name) -> ilias::Task<int>;
        auto stop_node(std::string_view name) -> ilias::Task<int>;
        auto stop() -> void;
        auto stop(const CommandParser::ArgsType &args, const CommandParser::OptionsType &options)
            -> std::string;

        template <typename T>
        auto set_option(std::string_view option, const T &value);
        template <typename T>
        auto get_option(std::string_view option) -> ilias::Result<T>;

        // commands
        auto start_console() -> ilias::Task<void>;
        auto stop_console() -> void;
        auto command_installer(std::string_view module)
            -> std::function<bool(CommandParser::CommandsData &&)>;

        // status
        auto show_status(const CommandParser::ArgsType    &args,
                         const CommandParser::OptionsType &options) -> std::string;

    private:
        // for server
        auto _accept_client(ilias::TcpClient client) -> ilias::Task<void>;

    private:
        bool                _isRuning           = false;
        bool                _isConsoleListening = false;
        ::ilias::IoContext *_ctx                = nullptr;

        CommandParser                                                 _commandParser;
        std::unordered_map<std::string, std::any>                     _optionsMap;
        std::unordered_map<std::string_view, NodeData>                _nodeMap;
        std::unordered_map<int, std::set<Consumer *>>                 _consumerMap;
        std::unordered_map<std::string_view, ilias::WaitHandle<void>> _cancelHandleMap;
        std::deque<std::string> _statusList; // For internal log storage
        size_t                  _statusListMaxSize = 100;
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
