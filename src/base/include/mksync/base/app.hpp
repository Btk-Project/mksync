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
#include <list>
#include <ilias/ilias.hpp>
#include <ilias/net/tcp.hpp>
#include <ilias/fs/console.hpp>
#include <fmt/format.h>
#include <cxxopts.hpp>

#include "mksync/base/command_invoker.hpp"
#include "mksync/base/mk_capture.hpp"
#include "mksync/base/mk_sender.hpp"
#include "mksync/base/base_library.h"
#include "mksync/base/nodebase.hpp"
#include "mksync/base/node_manager.hpp"
#include "mksync/base/settings.hpp"

namespace mks::base
{
    /**
     * @brief Application
     * - 负责管理软件的主要组件
     *      - 通信节点
     *      - 键鼠捕获节点
     *      - 监听控制台输入
     *      - 分发事件
     * - 全局上下文
     *      - 配置全局共享的选项
     *      - 注册命令
     *      - 设置日志
     */
    class MKS_BASE_API App {
    public:
        App(::ilias::IoContext *ctx);
        ~App();

        static auto app_name() -> const char *;
        static auto app_version() -> const char *;
        auto        get_io_context() const -> ::ilias::IoContext *;

        // main loop
        auto exec(int argc = 0, const char *const *argv = nullptr) -> ilias::Task<void>;
        auto stop() -> void;
        auto stop(const CommandInvoker::ArgsType &args, const CommandInvoker::OptionsType &options)
            -> std::string;
        auto settings() -> Settings &;
        auto node_manager() -> NodeManager &;

        // commands
        auto start_console() -> ilias::Task<void>;
        auto stop_console() -> void;
        auto command_installer(std::string_view module)
            -> std::function<bool(std::unique_ptr<Command>)>;

        // status
        auto log_handle(const CommandInvoker::ArgsType    &args,
                        const CommandInvoker::OptionsType &options) -> std::string;

    private:
        bool                _isRuning           = false;
        bool                _isConsoleListening = false;
        ::ilias::IoContext *_ctx                = nullptr;

        CommandInvoker          _commandInvoker;
        NodeManager             _nodeManager;
        Settings                _settings;
        std::deque<std::string> _logList; // For internal log storage
        size_t                  _logListMaxSize = 100;
    };
} // namespace mks::base
