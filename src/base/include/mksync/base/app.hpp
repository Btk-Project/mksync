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
#include <mksync/base/base_library.h>

#include <ilias/ilias.hpp>
#include <ilias/net/tcp.hpp>
#include <ilias/fs/console.hpp>
#include <fmt/format.h>
#include <cxxopts.hpp>

#include "mksync/base/node_manager.hpp"
#include "mksync/base/settings.hpp"
#include "mksync/proto/system_proto.hpp"
#include "mksync/base/communication.hpp"
#include "mksync/base/command.hpp"
#include "mksync/base/command_invoker.hpp"

MKS_BEGIN

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

        /**
         * @brief 获取ilias的 i/o context对象
         *
         * @return ::ilias::IoContext*
         */
        static auto app_name() -> const char *;

        /**
         * @brief 获取软件版本信息
         *
         * @return const char*
         */
        static auto app_version() -> const char *;

        /**
         * @brief 获取ilias的 i/o context对象
         *
         * @return ::ilias::IoContext*
         */
        virtual auto get_io_context() const -> ::ilias::IoContext * = 0;

        /**
         * @brief 获取本机的屏幕信息
         *
         * @return VirtualScreenInfo
         */
        virtual auto get_screen_info() const -> VirtualScreenInfo = 0;

        /**
         * @brief 获取Settings对象
         * @note 对settings对象的修改不会自动通知到其他模块
         * @return Settings&
         */
        virtual auto settings() -> Settings & = 0;

        /**
         * @brief 获取NodeManager对象
         * @note 对节点的修改会实时生效。
         * @return NodeManager&
         */
        virtual auto node_manager() -> NodeManager & = 0;

        /**
         * @brief 获取通信节点
         * @note
         * 获取的是通信节点的wapper，可能为空，需要判断，保险起见请在程序作为服务端或客户端后再获取。
         * @return ICommunication*
         */
        virtual auto communication() -> ICommunication * = 0;

        /**
         * @brief 获取命令执行器
         * @note 可以通过执行器直接执行字符串指令
         * @return CommandInvoker&
         */
        virtual auto command_invoker() -> CommandInvoker & = 0;

        /**
         * @brief 命令注册器
         * @note 注册需要访问节点名称，后续命令可以通过绑定的节点移除。
         * @param module
         */
        virtual auto command_installer(NodeBase *module)
            -> std::function<bool(std::unique_ptr<Command>)> = 0;

        /**
         * @brief 删除指定模块的所有命令
         *
         * @param module
         */
        virtual auto command_uninstaller(NodeBase *module) -> void = 0;

        /**
         * @brief 向主事件队列中推入一个来自未知模块的事件
         * @note 非线程安全接口
         * @param event
         * @return ::ilias::Task<::ilias::Error>
         */
        virtual auto push_event(NekoProto::IProto &&event) -> ::ilias::Task<::ilias::Error> = 0;

        /**
         * @brief 向主事件队列推入一个来自指定模块的事件
         * @note 非线程安全接口，指定模块可以防止事件传回自己。
         * @param event
         * @param node 推入事件的模块
         * @return ::ilias::Task<::ilias::Error>
         */
        virtual auto push_event(NekoProto::IProto &&event, NodeBase *node)
            -> ::ilias::Task<::ilias::Error> = 0;

        /**
         * @brief 主循环，软件的启动函数，将阻塞当前协程。
         *
         * @param argc argument count
         * @param argv argument vector
         * @return ilias::Task<void>
         */
        [[nodiscard("coroutine function")]]
        virtual auto exec(int argc = 0, const char *const *argv = nullptr) -> ilias::Task<void> = 0;

        /**
         * @brief 停止程序
         *
         * @return ilias::Task<void>
         */
        [[nodiscard("coroutine function")]]
        virtual auto stop() -> ilias::Task<void> = 0;
    };
MKS_END