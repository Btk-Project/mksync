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
#include <ilias/ilias.hpp>
#include <ilias/net/tcp.hpp>
#include <ilias/fs/console.hpp>
#include <ilias/sync/event.hpp>
#include <fmt/format.h>
#include <cxxopts.hpp>

#include "mksync/base/command_invoker.hpp"
#include "mksync/core/mk_capture.hpp"
#include "mksync/core/mk_sender.hpp"
#include "mksync/core/core_library.h"
#include "mksync/base/nodebase.hpp"
#include "mksync/base/node_manager.hpp"
#include "mksync/base/settings.hpp"
#include "mksync/proto/proto.hpp"
#include "mksync/base/app.hpp"
#include "mksync/core/communication.hpp"

namespace mks::base
{
    /**
     * @brief 软件核心类
     * @par 管理软件的主要组件
     *      - 通信节点
     *      - 监听控制台输入
     *      - 节点管理器
     *      - 命令执行器
     * @par 全局上下文
     *      - 配置全局选项
     *      - 注册命令
     *      - 设置日志
     *      - 获取主要模块
     */
    class MKS_CORE_API App final : public IApp {
    public:
        App(::ilias::IoContext *ctx);
        ~App();

        /**
         * @brief 获取ilias的 i/o context对象
         *
         * @return ::ilias::IoContext*
         */
        auto get_io_context() const -> ::ilias::IoContext * override;
        /**
         * @brief 获取本机的屏幕信息
         *
         * @return VirtualScreenInfo
         */
        auto get_screen_info() const -> VirtualScreenInfo override;

        /**
         * @brief 主循环，软件的启动函数，将阻塞当前协程。
         *
         * @param argc argument count
         * @param argv argument vector
         * @return ilias::Task<void>
         */
        [[nodiscard("coroutine function")]]
        auto exec(int argc = 0, const char *const *argv = nullptr) -> ilias::Task<void> override;

        /**
         * @brief 停止程序
         *
         * @return ilias::Task<void>
         */
        [[nodiscard("coroutine function")]]
        auto stop() -> ilias::Task<void> override;

        /**
         * @brief 停止程序
         * @note 这个函数由commandInvoker调用，在用户输入指令或远程发起指令 exit/quit... 时执行
         * @param args  忽略
         * @param options 忽略
         * @return ::ilias::Task<std::string>
         */
        [[nodiscard("coroutine function")]]
        auto stop(const CommandInvoker::ArgsType &args, const CommandInvoker::OptionsType &options)
            -> ::ilias::Task<std::string>;

        /**
         * @brief 获取Settings对象
         * @note 对settings对象的修改不会自动通知到其他模块
         * @return Settings&
         */
        auto settings() -> Settings & override;

        /**
         * @brief 获取NodeManager对象
         * @note 对节点的修改会实时生效。
         * @return NodeManager&
         */
        auto node_manager() -> NodeManager & override;

        /**
         * @brief 获取通信节点
         * @note
         * 获取的是通信节点的wapper，可能为空，需要判断，保险起见请在程序作为服务端或客户端后再获取。
         * @return ICommunication*
         */
        auto communication() -> ICommunication * override;

        // ----------- commands -------------
        /**
         * @brief 启动终端输入监听
         *
         * @return ilias::Task<void>
         */
        [[nodiscard("coroutine function")]]
        auto start_console() -> ilias::Task<void>;

        /**
         * @brief 停止终端输入监听
         *
         * @return ilias::Task<void>
         */
        [[nodiscard("coroutine function")]]
        auto stop_console() -> ilias::Task<void>;

        /**
         * @brief 命令注册器
         * @note 注册需要访问节点名称，后续命令可以通过绑定的节点移除。
         * @param module
         * @return std::function<bool(std::unique_ptr<Command>)>
         */
        auto command_installer(NodeBase *module)
            -> std::function<bool(std::unique_ptr<Command>)> override;

        /**
         * @brief 获取命令执行器
         *
         * @return CommandInvoker&
         */
        auto command_invoker() -> CommandInvoker & override;

        /**
         * @brief 删除指定模块的所有命令
         *
         * @param module
         */
        auto command_uninstaller(NodeBase *module) -> void override;

        /**
         * @brief 向主事件队列中推入一个来自未知模块的事件
         * @note 非线程安全接口
         * @param event
         * @return ::ilias::Task<::ilias::Error>
         */
        [[nodiscard("coroutine function")]]
        auto push_event(NekoProto::IProto &&event) -> ::ilias::Task<::ilias::Error> override;
        /**
         * @brief 向主事件队列推入一个来自指定模块的事件
         * @note 非线程安全接口，指定模块可以防止事件传回自己。
         * @param event
         * @param node 推入事件的模块
         * @return ::ilias::Task<::ilias::Error>
         */
        [[nodiscard("coroutine function")]]
        auto push_event(NekoProto::IProto &&event, NodeBase *node)
            -> ::ilias::Task<::ilias::Error> override;

        /**
         * @brief 处理日志命令
         *
         * @param args 参数列表
         * @param options 选项列表
         * @return ::ilias::Task<std::string>
         */
        [[nodiscard("coroutine function")]]
        auto log_handle(const CommandInvoker::ArgsType    &args,
                        const CommandInvoker::OptionsType &options) -> ::ilias::Task<std::string>;

    private:
        // 主循环控制
        bool           _isRuning = false;
        ::ilias::Event _exitEvent;

        // 控制台相关配置
        bool _isConsoleListening = false;
        bool _isNoConsole        = false;

        ::ilias::IoContext *_ctx           = nullptr; // ilias I/O 上下文
        MKCommunication    *_communication = nullptr; // 通信节点
        CommandInvoker      _commandInvoker;          // 命令执行器
        NodeManager         _nodeManager;             // 节点管理器
        Settings            _settings;                // 本地配置

        // 日志记录
        std::deque<std::string> _logList;              // 日志缓存
        size_t                  _logListMaxSize = 100; // 最大日志缓存条数
    };
} // namespace mks::base
