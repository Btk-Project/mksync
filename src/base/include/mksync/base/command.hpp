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

#include <nekoproto/proto/proto_base.hpp>
#include <vector>

#include "mksync/base/base_library.h"

namespace mks::base
{
    class MKS_BASE_API Command {
    public:
        Command()          = default;
        virtual ~Command() = default;

        /// @brief 执行命令
        virtual void execute() = 0;
        /// @brief 撤销命令
        virtual void undo() = 0;

        /// @brief 获取命令描述， 为空时返回命令的说明， 否则返回指定选项的说明
        virtual auto description(std::string_view option = "") const -> std::string = 0;
        /// @brief 获取命令选项
        virtual auto options() const -> std::vector<std::string> = 0;
        /// @brief 获取命令帮助信息, (完整的命令说明)
        virtual auto help() const -> std::string = 0;
        /// @brief 获取命令名称
        virtual auto name() const -> std::string_view                     = 0;
        virtual auto alias_names() const -> std::vector<std::string_view> = 0;
        /// @brief 设置选项的值
        virtual void set_option(std::string_view option, std::string_view value) = 0;
        virtual void set_options(const NekoProto::IProto& proto)                        = 0;
        /// @brief 获取选项的值
        virtual auto get_option(std::string_view option) const -> std::string = 0;
        virtual auto get_options() const -> NekoProto::IProto                 = 0;
    };
} // namespace mks::base