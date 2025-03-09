#include "mksync/base/command_invoker.hpp"

#include <spdlog/spdlog.h>
#include <charconv>
#include <fmt/format.h>

#include "mksync/base/app.hpp"
namespace mks::base
{
    using ::ilias::Task;

    auto CommonCommand::execute() -> Task<void>
    {
        if (auto ret = _data.callback(_args, _options); !ret.empty()) {
            SPDLOG_ERROR("{} failed : {}", name(), ret);
        }
        co_return;
    }

    auto CommonCommand::help(std::string_view indentation) const -> std::string
    {
        auto ret = fmt::format("{}{}", indentation, _data.command[0]);
        if (_data.command.size() > 1) {
            ret += "(";
            for (size_t i = 1; i < _data.command.size(); ++i) {
                ret += fmt::format("{}", _data.command[i]);
                if (i != _data.command.size() - 1) {
                    ret += ", ";
                }
            }
            ret += ")";
        }
        ret += fmt::format(":\n{0}{0}{1}\n", indentation, _data.description);
        const char *type;
        for (const auto &opt : _data.options) {
            switch (opt.type) {
            case CommandInvoker::OptionsData::eBool:
                type = "Bool";
                break;
            case CommandInvoker::OptionsData::eInt:
                type = "Int";
                break;
            case CommandInvoker::OptionsData::eDouble:
                type = "Float";
                break;
            case CommandInvoker::OptionsData::eString:
                type = "String";
                break;
            default:
                type = "Unknown";
            }
            ret += fmt::format("{0}{0}-{1}=$({2}) {3}\n", indentation, opt.name, type,
                               opt.description);
        }
        ret.pop_back();
        return ret;
    }

    auto CommonCommand::name() const -> std::string_view
    {
        return _data.command.front();
    }

    auto CommonCommand::alias_names() const -> std::vector<std::string_view>
    {
        return std::vector<std::string_view>(_data.command.begin() + 1, _data.command.end());
    }

    void CommonCommand::set_option(std::string_view option, std::string_view value)
    {
        if (option.empty()) {
            _args.push_back(value);
            SPDLOG_INFO("arg {}", value);
            return;
        }
        for (auto &opt : _data.options) {
            if (opt.name == option) {
                switch (opt.type) {
                case CommandInvoker::OptionsData::eBool: {
                    _options[opt.name] = (value == "true") || (value == "y");
                    SPDLOG_INFO("B {} = {}", opt.name, std::get<bool>(_options[opt.name]));
                } break;
                case CommandInvoker::OptionsData::eInt: {
                    int  value1 = 0;
                    auto ret = std::from_chars(value.data(), value.data() + value.length(), value1);
                    if (ret.ec != std::errc()) {
                        SPDLOG_ERROR("invalid option \"{}\"", opt.name);
                    }
                    _options[opt.name] = value1;
                    SPDLOG_INFO("I {} = {}", opt.name, std::get<int>(_options[opt.name]));
                } break;
                case CommandInvoker::OptionsData::eDouble: {
                    double value1 = 0;
                    auto ret = std::from_chars(value.data(), value.data() + value.length(), value1);
                    if (ret.ec != std::errc()) {
                        SPDLOG_ERROR("invalid option \"{}\"", opt.name);
                    }
                    _options[opt.name] = value1;
                    SPDLOG_INFO("D {} = {}", opt.name, std::get<double>(_options[opt.name]));
                } break;
                case CommandInvoker::OptionsData::eString: {
                    _options[opt.name] = std::string(value);
                    SPDLOG_INFO("S {} = {}", opt.name, std::get<std::string>(_options[opt.name]));
                } break;
                default:
                    SPDLOG_ERROR("invalid option {}", opt.name);
                    break;
                }
            }
        }
    }

    void CommonCommand::set_options([[maybe_unused]] const NekoProto::IProto &proto)
    {
        SPDLOG_ERROR("No command proto implemented for {}", name());
    }

    auto CommonCommand::get_option(std::string_view option) const -> std::string
    {
        auto item = _options.find(option);
        if (item != _options.end()) {
            return detail::to_string(item->second);
        }
        return std::string();
    }

    auto CommonCommand::get_options() const -> NekoProto::IProto
    {
        return NekoProto::IProto();
    }

    CommandInvoker::CommandInvoker(App *app) : _app(app)
    {
        install_cmd(std::make_unique<CommonCommand>(CommandsData{
                        {"help", "h", "?"},
                        "show help, can specify module name, e.g. help [$module1] ...",
                        std::bind(&CommandInvoker::show_help, this, std::placeholders::_1,
                                  std::placeholders::_2),
                        {}
        }),
                    "Core");
        install_cmd(std::make_unique<CommonCommand>(CommandsData{
                        {"version", "v"},
                        "show version",
                        std::bind(&CommandInvoker::show_version, this, std::placeholders::_1,
                                  std::placeholders::_2),
                        {}
        }),
                    "Core");
    }

    auto CommandInvoker::install_cmd(std::unique_ptr<Command> command, std::string_view module)
        -> bool
    {
        if (_trie.search(command->name())) {
            SPDLOG_ERROR("command \"{}\" already exists", command->name());
            return false;
        }
        for (const auto &cmd : command->alias_names()) {
            if (_trie.search(cmd)) {
                SPDLOG_WARN("command {}'s alias \"{}\" already exists, will override it.",
                            command->name(), cmd);
            }
        }
        _commands.emplace_back(std::move(command));
        _modules.insert({std::string(module), _commands.size() - 1});
        _trie.insert(_commands.back()->name(), (int)_commands.size() - 1);
        for (const auto &cmd : _commands.back()->alias_names()) {
            _trie.insert(cmd, (int)_commands.size() - 1);
        }
        auto proto = _commands.back()->get_options();
        if (proto != nullptr) {
            _protoCommandsTable[proto.type()] = (int)_commands.size() - 1;
        }
        return true;
    }

    auto CommandInvoker::execute(std::string_view cmd) -> Task<void>
    {
        co_return co_await execute(_split(cmd));
    }

    auto CommandInvoker::execute(std::vector<std::string_view> cmdline) -> Task<void>
    {
        if (cmdline.empty()) {
            co_return;
        }
        ArgsType args;
        auto     cmd   = cmdline.front();
        auto     item  = _trie.search(cmd);
        int      index = -1;
        if (item) {
            index = item.value();
            if (index < 0 && index >= (int)_commands.size()) {
                SPDLOG_ERROR("command \"{}\" not found", cmd);
                co_return;
            }
        }
        else {
            SPDLOG_ERROR("command \"{}\" not found", cmd);
            co_return;
        }

        for (int i = 1; i < (int)cmdline.size(); ++i) { // 将命令的选项与参数提取出来
            auto str = cmdline[i];
            if (str[0] == '-') {
                auto pos = str.find('=');
                if (pos != std::string::npos) {
                    _commands[index]->set_option(str.substr(1, pos - 1),
                                                 std::string(str.substr(pos + 1)));
                }
                else {
                    _commands[index]->set_option(str.substr(1), std::string("true"));
                }
            }
            else {
                _commands[index]->set_option("", str);
            }
        }
        co_return co_await _commands[index]->execute();
    }

    auto CommandInvoker::execute(const NekoProto::IProto &proto) -> Task<void>
    {
        // if (proto == nullptr) {
        //     return;
        // }
        int  type = proto.type();
        auto item = _protoCommandsTable.find(type);
        if (item != _protoCommandsTable.end()) {
            int index = item->second;
            if (index >= 0 && index < (int)_commands.size()) {
                _commands[index]->set_options(proto);
                co_return co_await _commands[index]->execute();
            }
        }
    }

    auto CommandInvoker::show_version([[maybe_unused]] const ArgsType    &args,
                                      [[maybe_unused]] const OptionsType &options) -> std::string
    {
        fprintf(stdout, "%s %s\n", App::app_name(), App::app_version());
        return "";
    }

    auto CommandInvoker::show_help([[maybe_unused]] const ArgsType    &args,
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
            fprintf(stdout, "%s\n", command->help("    ").c_str());
        }
        ::fflush(stdout);
        return "";
    }

    auto CommandInvoker::_split(std::string_view str, char ch) -> std::vector<std::string_view>
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