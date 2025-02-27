/**
 * @file command_parser.hpp
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
#include <set>

#include "mksync/base/trie.hpp"
#include "mksync/base/base_library.h"

namespace mks::base
{
    class App;

    class MKS_BASE_API CommandParser {
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
        CommandParser(App *app);
        auto install_cmd(const CommandsData &command, std::string_view module = "") -> bool;
        auto install_cmd(CommandsData &&command, std::string_view module = "") -> bool;
        auto parser(std::string_view cmdline) -> std::string;
        auto parser(std::vector<std::string_view> cmdline) -> std::string;
        auto show_help(const ArgsType &args, const OptionsType &options) -> std::string;
        auto show_version(const ArgsType &args, const OptionsType &options) -> std::string;

    private:
        static auto _parser(std::set<std::string_view> &opts, OptionsType &options,
                            const std::vector<OptionsData> &optdatas) -> bool;
        static auto _split(std::string_view str, char ch = ' ') -> std::vector<std::string_view>;

    private:
        std::unordered_multimap<std::string, int> _modules;
        std::vector<CommandsData>                 _commands;
        Trie<int>                                 _trie;
        App                                      *_app = nullptr;
    };
} // namespace mks::base