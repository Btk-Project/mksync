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

#include "mksync/base/base_library.h"
#include "mksync/base/node_manager.hpp"
#include "mksync/base/settings.hpp"
#include "mksync/proto/proto.hpp"
#include "mksync/base/communication.hpp"
#include "mksync/base/command.hpp"

namespace mks::base
{
    /**
     * @brief Application
     * - 全局上下文
     *      - 配置全局共享的选项
     *      - 注册命令
     *      - 设置日志
     *      - 节点管理
     */
    class MKS_BASE_API IApp {
    public:
        IApp()          = default;
        virtual ~IApp() = default;

        static auto  app_name() -> const char *;
        static auto  app_version() -> const char *;
        virtual auto get_io_context() const -> ::ilias::IoContext * = 0;
        virtual auto get_screen_info() const -> VirtualScreenInfo   = 0;
        virtual auto settings() -> Settings &                       = 0;
        virtual auto node_manager() -> NodeManager &                = 0;
        virtual auto communication() -> ICommunication *            = 0;
        virtual auto command_installer(std::string_view module)
            -> std::function<bool(std::unique_ptr<Command>)> = 0;

        // main loop
        [[nodiscard("coroutine function")]]
        virtual auto exec(int argc = 0, const char *const *argv = nullptr) -> ilias::Task<void> = 0;
        [[nodiscard("coroutine function")]]
        virtual auto stop() -> ilias::Task<void> = 0;
    };
} // namespace mks::base
