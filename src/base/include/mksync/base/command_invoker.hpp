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

#include "mksync/base/trie.hpp"
#include "mksync/base/base_library.h"
#include "mksync/base/command.hpp"

namespace mks::base
{
    class App;

    class MKS_BASE_API CommandInvoker {
    public:
        using CommandsType = std::vector<std::string_view>;
        using ArgsType     = std::vector<std::string_view>;
        using OptionsType =
            std::map<std::string_view, std::variant<bool, int, double, std::string>>;
        using CommandCallbackType =
            std::function<std::string(const ArgsType &args, const OptionsType &options)>;
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
        CommandInvoker(App *app);
        auto install_cmd(std::unique_ptr<Command> command, std::string_view module = "") -> bool;
        auto execute(std::string_view cmdline) -> ::ilias::Task<void>;
        auto execute(std::vector<std::string_view> cmdline) -> ::ilias::Task<void>;
        auto execute(const NekoProto::IProto &proto) -> ::ilias::Task<void>;

        auto show_help(const ArgsType &args, const OptionsType &options) -> std::string;
        auto show_version(const ArgsType &args, const OptionsType &options) -> std::string;

    private:
        static auto _split(std::string_view str, char ch = ' ') -> std::vector<std::string_view>;

    private:
        std::unordered_multimap<std::string, int> _modules;
        std::vector<std::unique_ptr<Command>>     _commands;
        std::unordered_map<int, int>              _protoCommandsTable;
        Trie<int>                                 _trie;
        App                                      *_app = nullptr;
    };

    class CommonCommand : public Command {
    public:
        CommonCommand(CommandInvoker::CommandsData &&data) : _data(data) {}
        auto execute() -> ::ilias::Task<void> override;
        auto help(std::string_view indentation) const -> std::string override;
        auto name() const -> std::string_view override;
        auto alias_names() const -> std::vector<std::string_view> override;
        void set_option(std::string_view option, std::string_view value) override;
        void set_options(const NekoProto::IProto &proto) override;
        auto get_option(std::string_view option) const -> std::string override;
        auto get_options() const -> NekoProto::IProto override;

    private:
        CommandInvoker::CommandsData _data;
        CommandInvoker::ArgsType     _args;
        CommandInvoker::OptionsType  _options;
    };

    namespace detail
    {
        template <typename T>
        std::string to_string(const T &value)
        {
            if constexpr (std::is_same<T, std::string>::value) {
                return value;
            }
            else if constexpr (std::is_same<T, std::string_view>::value) {
                return std::string(value);
            }
            else if constexpr (std::is_same<T, bool>::value) {
                return value ? "true" : "false";
            }
            else {
                return std::to_string(value);
            }
        }

        template <typename... Ts>
        struct unfold_option_variant_to_string_helper { // NOLINT(readability-identifier-naming)
            template <typename T, std::size_t N>
            static std::string unfold_value_imp2(const std::variant<Ts...> &value)
            {
                if (value.index() != N) {
                    return "";
                }
                return to_string(std::get<N>(value));
            }

            template <std::size_t... Ns>
            static std::string unfold_value(const std::variant<Ts...> &value,
                                            std::index_sequence<Ns...> /*unused*/)
            {
                return (unfold_value_imp2<std::variant_alternative_t<Ns, std::variant<Ts...>>, Ns>(
                            value) +
                        ...);
            }
        };

        template <typename... Ts>
        std::string to_string(const std::variant<Ts...> &value)
        {
            return unfold_option_variant_to_string_helper<Ts...>::unfold_value(
                value, std::index_sequence_for<Ts...>{});
        }
    } // namespace detail
} // namespace mks::base