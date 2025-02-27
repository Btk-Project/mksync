#include "mksync/base/command_parser.hpp"

#include <spdlog/spdlog.h>
#include <charconv>
#include <fmt/format.h>

#include "mksync/base/app.hpp"
namespace mks::base
{
    CommandParser::CommandParser(App *app) : _app(app)
    {
        install_cmd(
            {
                {"help", "h", "?"},
                "show help, can specify module name, e.g. help [$module1] ...",
                std::bind(&CommandParser::show_help, this, std::placeholders::_1,
                          std::placeholders::_2),
                {}
        },
            "Core");
        install_cmd(
            {
                {"version", "v"},
                "show version",
                std::bind(&CommandParser::show_version, this, std::placeholders::_1,
                          std::placeholders::_2),
                {}
        },
            "Core");
    }

    auto CommandParser::install_cmd(const CommandsData &command, std::string_view module) -> bool
    {
        for (const auto &cmd : command.command) {
            if (_trie.search(cmd)) {
                SPDLOG_ERROR("command \"{}\" already exists", cmd);
                return false;
            }
        }
        _commands.push_back(command);
        _modules.insert({std::string(module), _commands.size() - 1});
        for (const auto &cmd : command.command) {
            _trie.insert(cmd, (int)_commands.size() - 1);
        }
        return true;
    }

    auto CommandParser::install_cmd(CommandsData &&command, std::string_view module) -> bool
    {
        for (const auto &cmd : command.command) {
            if (_trie.search(cmd)) {
                SPDLOG_ERROR("command \"{}\" already exists", cmd);
                return false;
            }
        }
        _commands.emplace_back(std::forward<CommandsData>(command));
        _modules.insert({std::string(module), _commands.size() - 1});
        for (const auto &cmd : _commands.back().command) {
            _trie.insert(cmd, (int)_commands.size() - 1);
        }
        return true;
    }

    auto CommandParser::parser(std::string_view cmd) -> std::string
    {
        return parser(_split(cmd));
    }
    auto CommandParser::_parser(std::set<std::string_view> &opts, OptionsType &options,
                                const std::vector<OptionsData> &optdatas) -> bool
    {
        for (const auto &opt : optdatas) {
            if (opts.erase(opt.name) != 0U) {
                switch (opt.type) {
                case OptionsData::eBool: {
                    auto str          = std::get<std::string>(options[opt.name]);
                    options[opt.name] = (str == "true") || (str == "y");
                } break;
                case OptionsData::eInt: {
                    int  value = 0;
                    auto str   = std::get<std::string>(options[opt.name]);
                    auto ret   = std::from_chars(str.data(), str.data() + str.length(), value);
                    if (ret.ec != std::errc()) {
                        SPDLOG_ERROR("invalid option \"{}\"", opt.name);
                    }
                    options[opt.name] = value;
                } break;
                case OptionsData::eDouble: {
                    double value = 0;
                    auto   str   = std::get<std::string>(options[opt.name]);
                    auto   ret   = std::from_chars(str.data(), str.data() + str.length(), value);
                    if (ret.ec != std::errc()) {
                        SPDLOG_ERROR("invalid option \"{}\"", opt.name);
                    }
                    options[opt.name] = value;
                } break;
                default:
                    break;
                }
            }
        }
        if (opts.size() > 0) {
            SPDLOG_ERROR("unknown options: {}", fmt::join(opts, ","));
            return false;
        }
        return true;
    }

    auto CommandParser::parser(std::vector<std::string_view> cmdline) -> std::string
    {
        if (cmdline.empty()) {
            return "";
        }
        ArgsType                   args;
        OptionsType                options;
        auto                       cmd = cmdline.front();
        std::set<std::string_view> opts;
        for (int i = 1; i < (int)cmdline.size(); ++i) { // 将命令的选项与参数提取出来
            auto str = cmdline[i];
            if (str[0] == '-') {
                auto pos = str.find('=');
                if (pos != std::string::npos) {
                    options[str.substr(1, pos - 1)] = std::string(str.substr(pos + 1));
                }
                else {
                    options[str.substr(1)] = std::string("true");
                }
                opts.insert(str.substr(1, pos - 1));
            }
            else {
                args.push_back(str);
            }
        }
        auto item = _trie.search(cmd);
        if (item) {
            int index = item.value();
            _parser(opts, options, _commands[index].options);
            if (index >= 0 && index < (int)_commands.size()) {
                return _commands[index].callback(args, options);
            }
        }
        SPDLOG_ERROR("command \"{}\" not found", cmd);
        return "";
    }

    auto CommandParser::show_version([[maybe_unused]] const ArgsType    &args,
                                     [[maybe_unused]] const OptionsType &options) -> std::string
    {
        fprintf(stdout, "%s %s\n", App::app_name(), App::app_version());
        return "";
    }

    auto CommandParser::show_help([[maybe_unused]] const ArgsType    &args,
                                  [[maybe_unused]] const OptionsType &options) -> std::string
    {
        fprintf(stdout, "Usage: $%s [command] [args] [options]\n", App::app_name());
        std::string module;
        for (auto item = _modules.begin(); item != _modules.end(); ++item) {
            if (!args.empty() &&
                !std::ranges::any_of(args.begin(), args.end(), [&item](std::string_view arg) {
                    std::string i1(arg);
                    std::string i2(item->first);
                    std::transform(i1.begin(), i1.end(), i1.begin(), ::tolower);
                    std::transform(i2.begin(), i2.end(), i2.begin(), ::tolower);
                    return i1 == i2;
                })) {
                continue;
            }
            if (module != item->first) {
                module = item->first;
                fprintf(stdout, "\n%s:\n", module.c_str());
            }
            const auto &command = _commands[item->second];
            fprintf(stdout, "    %s", std::string(command.command[0]).c_str());
            if (command.command.size() > 1) {
                fprintf(stdout, "(");
                for (size_t i = 1; i < command.command.size(); ++i) {
                    fprintf(stdout, "%s", std::string(command.command[i]).c_str());
                    if (i != command.command.size() - 1) {
                        fprintf(stdout, ", ");
                    }
                }
                fprintf(stdout, ")");
            }
            fprintf(stdout, ":\n        %s\n", command.description.c_str());
            const char *type;
            for (const auto &opt : command.options) {
                switch (opt.type) {
                case OptionsData::eBool:
                    type = "Bool";
                    break;
                case OptionsData::eInt:
                    type = "Int";
                    break;
                case OptionsData::eDouble:
                    type = "Float";
                    break;
                case OptionsData::eString:
                    type = "String";
                    break;
                default:
                    type = "Unknown";
                }
                fprintf(stdout, "        -%s=$(%s) %s\n", opt.name.c_str(), type,
                        opt.description.c_str());
            }
        }
        ::fflush(stdout);
        return "";
    }

    auto CommandParser::_split(std::string_view str, char ch) -> std::vector<std::string_view>
    { // 拆分字符串,像"a b c d e"到{"a", "b", "c", "d", "e"}。
        std::vector<std::string_view> res;
        std::string_view              sv(str);
        while (!sv.empty()) {
            auto pos = sv.find(ch);
            if (pos != std::string_view::npos) {
                res.emplace_back(sv.substr(0, pos));
                sv.remove_prefix(pos + 1);
            }
            else {
                res.emplace_back(sv);
                sv.remove_prefix(sv.size());
            }
        }
        return res;
    }
} // namespace mks::base