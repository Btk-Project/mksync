/**
 * @file command.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-03-08
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once
#include <mksync/base/base_library.h>

#include <vector>

#include <ilias/task.hpp>
#include <nekoproto/proto/proto_base.hpp>
#include <cxxopts.hpp>

MKS_BEGIN

/**
 * @brief Command
 * - 命令基类
 * 定义了命令类的基础接口。
 *
 * @par 接口
 * - execute
 * 执行命令，协程函数，不可以阻塞。会在主线程启动的各个获取事件的协程中直接调用。
 * - help
 * 返回命令帮助信息, 完整的命令说明，需要包含缩进, 结尾需要换行。
 * - name
 * 获取命令名称,
 * 该名称不可以重复, 只有输入的命令完全匹配才会使用该类解析并执行。区分大小写, 只能包含字母。
 * - alias_names
 * 命令的别称集合，不需要包含name。如果没有可以返回空。
 * - set_option
 * 通过选项名设置选项的值。如果选择名为空则视为无名参数。
 * - set_options
 * 通过协议设置选项, 如果传入错误的协议可以无视, 警告, 断言或者抛出异常。通常,
 * 该接口由分发器自动调用, 协议类型应当同get_options返回的一致。
 * - parser_options
 * 解析来自命令行的输入，通常，该接口由分发器自动调用。
 * - get_option
 * 获取选项的值，如果选项不存在则返回空字符串。
 * - get_options
 * 构建协议并打包所有选项的值到协议中,
 * 如果未实现对应协议可以返回空协议, 此时将不会有协议分发到该命令。
 */
class MKS_BASE_API Command {
public:
    Command()          = default;
    virtual ~Command() = default;

    /// @brief 执行命令
    virtual auto execute() -> ilias::Task<std::string> = 0;

    /// @brief 获取命令帮助信息, 完整的命令说明
    virtual auto help() const -> std::string = 0;
    /// @brief 获取命令名称
    virtual auto name() const -> std::string_view = 0;
    /// @brief 命令的别称集合，不需要包含name.
    virtual auto alias_names() const -> std::vector<std::string_view> = 0;
    /// @brief 通过选项名设置选项的值。如果选择名为空则视为无名参数。
    virtual void set_option(std::string_view option, std::string_view value) = 0;
    /// @brief 通过协议设置选项的值，不识别的协议将不会做任何处理。
    virtual void set_options(const NekoProto::IProto &proto) = 0;
    /// @brief 解析来自命令行的输入
    virtual void parser_options(const std::vector<const char *> &args) = 0;
    /// @brief 可以使用的协议版本，为0时不绑定协议。
    virtual auto need_proto_type() const -> int = 0;
};

MKS_END