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
#include <mksync/base/base_library.h>

#include <string>
#include <vector>
#include <functional>
#include <map>

#include <nekoproto/proto/proto_base.hpp>
#include <cxxopts.hpp>

#include "mksync/base/trie.hpp"
#include "mksync/base/command.hpp"
#include "mksync/base/nodebase.hpp"

MKS_BEGIN

class IApp;
class NodeBase;

/**
 * @brief CommandInvoker
 * - 管理命令
 * - 通过事件/命令行输入执行命令
 */
class MKS_BASE_API CommandInvoker : public NodeBase, public Consumer {
public:
    using CommandsType = std::vector<std::string_view>;
    using ArgsType     = std::vector<std::string>;
    using OptionsType =
        std::map<std::string, std::variant<bool, int, double, std::string>, std::less<>>;
    using CommandCallbackType =
        std::function<::ilias::Task<std::string>(const ArgsType &args, const OptionsType &options)>;
    ///> 选项类型
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

    /**
     * @brief 初始化
     *
     * @return ::ilias::Task<int>
     */
    [[nodiscard("coroutine function")]]
    auto setup() -> ::ilias::Task<int> override;
    /**
     * @brief 清理
     *
     * @return ::ilias::Task<int>
     */
    [[nodiscard("coroutine function")]]
    auto teardown() -> ::ilias::Task<int> override;
    /**
     * @brief 获取模块名称
     *
     * @return const char*
     */
    auto name() -> const char * override;
    /**
     * @brief 获取模块订阅的事件
     *
     * @return std::vector<int>
     */
    auto get_subscribes() -> std::vector<int> override;
    /**
     * @brief 处理订阅的事件
     *
     * @param event
     * @return ::ilias::Task<void>
     */
    [[nodiscard("coroutine function")]]
    auto handle_event(const NekoProto::IProto &event) -> ::ilias::Task<void> override;

    /**
     * @brief 注册命令
     *
     * @param command 命令对象
     * @param module 模块指针
     * @return true
     * @return false 重复注册相同的命令会导致失败
     */
    auto install_cmd(std::unique_ptr<Command> command, NodeBase *module = nullptr) -> bool;
    /**
     * @brief 移除绑定到模块的命令
     *
     * @param module 模块指针
     */
    auto remove_cmd(NodeBase *module) -> void;
    /**
     * @brief 移除命令
     *
     * @param cmd 命令名称
     */
    auto remove_cmd(std::string_view cmd) -> void;

    /**
     * @brief 执行命令
     *
     * @param cmdline 命令字符串
     * @return ::ilias::Task<std::string>
     */
    [[nodiscard("coroutine function")]]
    auto execute(std::span<char> cmdline) -> ::ilias::Task<std::string>;

    /**
     * @brief 执行命令
     *
     * @param proto 协议
     * @return ::ilias::Task<std::string>
     */
    [[nodiscard("coroutine function")]]
    auto execute(const NekoProto::IProto &proto) -> ::ilias::Task<std::string>;

    /**
     * @brief 执行命令
     *
     * @param cmdline 命令行输入
     * @return ::ilias::Task<std::string>
     */
    [[nodiscard("coroutine function")]]
    auto execute(const std::vector<const char *> &cmdline) -> ::ilias::Task<std::string>;

    /**
     * @brief 显示帮助信息
     *
     * @param args 可以通过参数指定模块
     * @param options 忽视
     * @return ::ilias::Task<std::string>
     */
    [[nodiscard("coroutine function")]]
    auto show_help(const ArgsType &args, const OptionsType &options) -> ::ilias::Task<std::string>;

    /**
     * @brief 显示版本信息
     *
     * @param args 忽视
     * @param options 忽视
     * @return ::ilias::Task<std::string>
     */
    [[nodiscard("coroutine function")]]
    auto show_version(const ArgsType &args, const OptionsType &options)
        -> ::ilias::Task<std::string>;

private:
    /**
     * @brief 按分割符拆分字符串。
     * @note 该函数不会复制字符串，而是返回拆分出来的每个字符串的首字符地址，并将分割符替换成 0!
     * @param str 字符串
     * @param ch 分割符
     * @return std::vector<const char *>
     */
    static auto _split(std::span<char> str, char ch = ' ') -> std::vector<const char *>;

private:
    IApp                                               *_app = nullptr;
    std::list<std::unique_ptr<Command>>                 _commands; // 命令对象列表
    Trie<std::list<std::unique_ptr<Command>>::iterator> _trie;     // 命令到命令对象迭代器的映射
    std::unordered_map<int, std::list<std::unique_ptr<Command>>::iterator>
        _protoCommandsTable; // 协议type到命令对象迭代器的映射
    std::multimap<std::string, std::list<std::unique_ptr<Command>>::iterator>
        _modules; // 模块名到命令对象迭代器的映射
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

MKS_END