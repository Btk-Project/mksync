/**
 * @file command_invoker.hpp
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
#include <functional>
#include <map>
#include <nekoproto/proto/proto_base.hpp>
#include <cxxopts.hpp>

#include "mksync/base/trie.hpp"
#include "mksync/base/base_library.h"
#include "mksync/base/command.hpp"
#include "mksync/base/nodebase.hpp"

namespace mks::base
{
    class IApp;
    class NodeBase;

    /**
     * @brief CommandInvoker
     * - 命令调用器，管理并执行命令
     *      - 注册命令
     *      - 解析命令行
     *      - 通过事件/命令行输入执行命令
     */
    class MKS_BASE_API CommandInvoker : public NodeBase, public Consumer {
    public:
        using CommandsType = std::vector<std::string_view>;
        using ArgsType     = std::vector<std::string>;
        using OptionsType =
            std::map<std::string, std::variant<bool, int, double, std::string>, std::less<>>;
        using CommandCallbackType = std::function<::ilias::Task<std::string>(
            const ArgsType &args, const OptionsType &options)>;
        struct OptionsData {
            enum OptionType
            {
                eBool   = 0,
                eInt    = 1,
                eDouble = 2,
                eString = 3,
            };
            std::string name;
            OptionType  type;
            std::string description;
        };
        struct CommandsData {
            CommandsType             command;
            std::string              description;
            CommandCallbackType      callback;
            std::vector<OptionsData> options;
        };

    public:
        CommandInvoker(IApp *app);

        [[nodiscard("coroutine function")]]
        auto setup() -> ::ilias::Task<int> override;
        [[nodiscard("coroutine function")]]
        auto teardown() -> ::ilias::Task<int> override;
        auto name() -> const char * override;
        auto get_subscribes() -> std::vector<int> override;
        [[nodiscard("coroutine function")]]
        auto handle_event(const NekoProto::IProto &event) -> ::ilias::Task<void> override;

        auto install_cmd(std::unique_ptr<Command> command, NodeBase *module = nullptr) -> bool;
        auto remove_cmd(NodeBase *module) -> void;
        // 将删除其对应模块的所有命令。
        auto remove_cmd(std::string_view cmd) -> void;
        [[nodiscard("coroutine function")]]
        auto execute(std::span<char> cmdline) -> ::ilias::Task<std::string>;
        [[nodiscard("coroutine function")]]
        auto execute(const NekoProto::IProto &proto) -> ::ilias::Task<std::string>;
        [[nodiscard("coroutine function")]]
        auto execute(const std::vector<const char *> &cmdline) -> ::ilias::Task<std::string>;
        [[nodiscard("coroutine function")]]
        auto show_help(const ArgsType &args, const OptionsType &options)
            -> ::ilias::Task<std::string>;
        [[nodiscard("coroutine function")]]
        auto show_version(const ArgsType &args, const OptionsType &options)
            -> ::ilias::Task<std::string>;

    private:
        static auto _split(std::span<char> str, char ch = ' ') -> std::vector<const char *>;

    private:
        IApp                                                                  *_app = nullptr;
        std::list<std::unique_ptr<Command>>                                    _commands;
        Trie<std::list<std::unique_ptr<Command>>::iterator>                    _trie;
        std::unordered_map<int, std::list<std::unique_ptr<Command>>::iterator> _protoCommandsTable;
        std::multimap<std::string, std::list<std::unique_ptr<Command>>::iterator> _modules;
    };

    class CommonCommand : public Command {
    public:
        CommonCommand(CommandInvoker::CommandsData &&data);
        [[nodiscard("coroutine function")]]
        auto execute() -> ::ilias::Task<std::string> override;

        auto help() const -> std::string override;
        auto name() const -> std::string_view override;
        auto alias_names() const -> std::vector<std::string_view> override;
        void set_option(std::string_view option, std::string_view value) override;
        void set_options(const NekoProto::IProto &proto) override;
        void parser_options(const std::vector<const char *> &args) override;
        auto need_proto_type() const -> int override;

    private:
        CommandInvoker::CommandsData _data    = {};
        CommandInvoker::ArgsType     _args    = {};
        CommandInvoker::OptionsType  _options = {};
        mutable cxxopts::Options     _option;
    };
} // namespace mks::base