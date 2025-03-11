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

#include <ilias/task.hpp>
#include <nekoproto/proto/proto_base.hpp>
#include <vector>
#include <cxxopts.hpp>

#include "mksync/base/base_library.h"

namespace mks::base
{
    class MKS_BASE_API Command {
    public:
        Command()          = default;
        virtual ~Command() = default;

        /// @brief 执行命令
        virtual auto execute() -> ilias::Task<void> = 0;

        /// @brief 获取命令帮助信息, (完整的命令说明)
        virtual auto help() const -> std::string = 0;
        /// @brief 获取命令名称
        virtual auto name() const -> std::string_view = 0;
        /// @brief 命令的别称集合，不需要包含name.
        virtual auto alias_names() const -> std::vector<std::string_view> = 0;
        /// @brief 通过选项名设置选项的值。如果选择名为空则视为无名参数。
        virtual void set_option(std::string_view option, std::string_view value) = 0;
        /// @brief 通过协议设置选项的值（不识别的协议将不会做任何处理。）。
        virtual void set_options(const NekoProto::IProto &proto) = 0;
        /// @brief 解析来自命令行的输入
        virtual void parser_options(const std::vector<const char *> &args) = 0;
        /// @brief 获取选项的值
        virtual auto get_option(std::string_view option) const -> std::string = 0;
        /// @brief 打包所有选项的值到协议中（如果未实现对应协议将返回空。）。
        virtual auto get_options() const -> NekoProto::IProto = 0;
    };
} // namespace mks::base