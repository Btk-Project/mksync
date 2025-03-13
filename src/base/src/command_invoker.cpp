#include "mksync/base/command_invoker.hpp"

#include <spdlog/spdlog.h>
#include <charconv>
#include <fmt/format.h>

#include "mksync/base/app.hpp"
namespace mks::base
{
    using ::ilias::Task;

    namespace detail
    {
        inline std::string to_string(bool value)
        {
            return value ? "true" : "false";
        }

        template <typename T>
            requires std::is_convertible_v<T, std::string> ||
                     std::is_constructible_v<std::string, T>
        std::string to_string(const T &value)
        {
            return std::string(value);
        }

        template <typename T>
            requires requires(T va) { std::to_string(va); }
        std::string to_string(const T &value)
        {
            return std::to_string(value);
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

    CommonCommand::CommonCommand(CommandInvoker::CommandsData &&data)
        : _data(data), _option(std::string(_data.command[0]))
    {
        for (auto &opt : _data.options) {
            switch (opt.type) {
            case CommandInvoker::OptionsData::eBool:
                _option.add_option("", {opt.name, opt.description, cxxopts::value<bool>(), ""});
                break;
            case CommandInvoker::OptionsData::eString:
                _option.add_option(
                    "", {opt.name, opt.description, cxxopts::value<std::string>(), "[string]"});
                break;
            case CommandInvoker::OptionsData::eInt:
                _option.add_option("", {opt.name, opt.description, cxxopts::value<int>(), "[int]"});
                break;
            case CommandInvoker::OptionsData::eDouble:
                _option.add_option(
                    "", {opt.name, opt.description, cxxopts::value<double>(), "[double]"});
                break;
            }
        }
        _option.custom_help(_data.description);
        _option.allow_unrecognised_options();
    }

    auto CommonCommand::execute() -> Task<void>
    {
        if (auto ret = _data.callback(_args, _options); !ret.empty()) {
            SPDLOG_ERROR("{} failed : {}", name(), ret);
        }
        co_return;
    }

    auto CommonCommand::help() const -> std::string
    {
        return _option.help({}, false);
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

    void CommonCommand::parser_options(const std::vector<const char *> &args)
    {
        auto options = _option.parse(int(args.size()), args.data());
        for (const auto &opt : _data.options) {
            if (auto size = options.count(opt.name); size == 0) {
                continue;
            }
            switch (opt.type) {
            case CommandInvoker::OptionsData::eBool:
                _options[opt.name] = options[opt.name].as<bool>();
                break;
            case CommandInvoker::OptionsData::eInt:
                _options[opt.name] = options[opt.name].as<int>();
                break;
            case CommandInvoker::OptionsData::eDouble:
                _options[opt.name] = options[opt.name].as<double>();
                break;
            case CommandInvoker::OptionsData::eString:
                _options[opt.name] = options[opt.name].as<std::string>();
                break;
            }
        }
        for (auto arg : options.unmatched()) {
            _args.push_back(arg);
        }
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
                        "help(h,?) : show help, can specify module name, e.g. help [$module1] ...",
                        std::bind(&CommandInvoker::show_help, this, std::placeholders::_1,
                                  std::placeholders::_2),
                        {}
        }),
                    "Core");
        install_cmd(std::make_unique<CommonCommand>(CommandsData{
                        {"version", "v"},
                        "version(v) : show version",
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

    auto CommandInvoker::execute(std::span<char> cmd) -> Task<void>
    {
        co_return co_await execute(_split(cmd));
    }

    auto CommandInvoker::execute(const std::vector<const char *> &cmdline) -> Task<void>
    {
        if (cmdline.empty()) {
            co_return;
        }
        const auto *cmd   = cmdline.front();
        auto        item  = _trie.search(cmd);
        int         index = -1;
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
        _commands[index]->parser_options(cmdline);
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
                fprintf(stdout, "%s:\n", module.c_str());
            }
            const auto &command = _commands[item->second];
            auto        help    = command->help();
            fprintf(stdout, "%s%s", help.c_str(),
                    ((help.size() > 2 && help[help.size() - 2] != '\n') ? "\n" : ""));
        }
        ::fflush(stdout);
        return "";
    }

    auto CommandInvoker::_split(std::span<char> str, char ch) -> std::vector<const char *>
    { // 拆分字符串,像"a b c d e"到{"a", "b", "c", "d", "e"}。
        std::vector<const char *> res;
        std::string_view          sv(str.data(), str.size());
        std::size_t               pos = 0;
        while (pos < sv.size()) {
            auto spos = pos;
            pos       = sv.find(ch, spos);
            if (pos != std::string_view::npos) {
                res.emplace_back(sv.substr(spos, pos - spos).data());
                str[pos] = '\0';
                pos++;
            }
            else {
                res.emplace_back(sv.substr(spos).data());
            }
        }
        return res;
    }
} // namespace mks::base