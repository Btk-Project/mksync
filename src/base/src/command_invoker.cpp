#include "mksync/base/command_invoker.hpp"

#include <charconv>
#include <algorithm>
#include <numeric>

#include <spdlog/spdlog.h>
#include <fmt/format.h>

#include "mksync/base/app.hpp"

MKS_BEGIN

using ::ilias::Task;

namespace detail
{
    inline std::string to_string(bool value)
    {
        return value ? "true" : "false";
    }

    template <typename T>
        requires std::is_convertible_v<T, std::string> || std::is_constructible_v<std::string, T>
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
            return (
                unfold_value_imp2<std::variant_alternative_t<Ns, std::variant<Ts...>>, Ns>(value) +
                ...);
        }
    };

    template <typename... Ts>
    std::string to_string(const std::variant<Ts...> &value)
    {
        return unfold_option_variant_to_string_helper<Ts...>::unfold_value(
            value, std::index_sequence_for<Ts...>{});
    }

    template <typename T>
    std::string to_string(const std::vector<T> &value)
    {
        return std::accumulate(
            value.begin(), value.end(), std::string{}, [](const std::string &astr, const T &bstr) {
                return astr.empty() ? to_string(bstr) : astr + "," + to_string(bstr);
            });
    }

    template <typename KeyT, typename ValueT, typename CompT>
    std::string to_string(const std::map<KeyT, ValueT, CompT> &value)
    {
        return std::accumulate(value.begin(), value.end(), std::string{},
                               [](const std::string &astr, const std::pair<KeyT, ValueT> &bstr) {
                                   return astr.empty()
                                              ? to_string(bstr.first) + "=" + to_string(bstr.second)
                                              : astr + "," + to_string(bstr.first) + "=" +
                                                    to_string(bstr.second);
                               });
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
            _option.add_option("",
                               {opt.name, opt.description, cxxopts::value<double>(), "[double]"});
            break;
        }
    }
    _option.custom_help(_data.description);
    _option.allow_unrecognised_options();
}

auto CommonCommand::execute() -> Task<std::string>
{
    co_return co_await _data.callback(_args, _options);
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
        _args.push_back(std::string(value));
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
                auto ret    = std::from_chars(value.data(), value.data() + value.length(), value1);
                if (ret.ec != std::errc()) {
                    SPDLOG_ERROR("invalid option \"{}\"", opt.name);
                }
                _options[opt.name] = value1;
                SPDLOG_INFO("I {} = {}", opt.name, std::get<int>(_options[opt.name]));
            } break;
            case CommandInvoker::OptionsData::eDouble: {
                double value1 = 0;
                auto   ret = std::from_chars(value.data(), value.data() + value.length(), value1);
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
    _args.clear();
    _options.clear();
    SPDLOG_ERROR("No command proto implemented for {}", name());
}

void CommonCommand::parser_options(const std::vector<const char *> &args)
{
    _args.clear();
    _options.clear();
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

auto CommonCommand::need_proto_type() const -> int
{
    return 0;
}

CommandInvoker::CommandInvoker(IApp *app) : _app(app)
{
    install_cmd(std::make_unique<CommonCommand>(CommandsData{
                    {"help", "h", "?"},
                    "help(h,?) : show help, can specify module name, e.g. help [$module1] ...",
                    std::bind(&CommandInvoker::show_help, this, std::placeholders::_1,
                              std::placeholders::_2),
                    {}
    }),
                nullptr);
    install_cmd(std::make_unique<CommonCommand>(CommandsData{
                    {"version", "v"},
                    "version(v) : show version",
                    std::bind(&CommandInvoker::show_version, this, std::placeholders::_1,
                              std::placeholders::_2),
                    {}
    }),
                nullptr);
}

auto CommandInvoker::install_cmd(std::unique_ptr<Command> command, NodeBase *module) -> bool
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
    auto item = _commands.emplace(_commands.end(), std::move(command));
    _modules.insert({std::string(module == nullptr ? "Core" : module->name()), item});
    _trie.insert(_commands.back()->name(), item);
    SPDLOG_INFO("install command \"{}\" for module {}<{}>", (*item)->name(),
                module == nullptr ? "Core" : module->name(), (void *)module);
    for (const auto &cmd : _commands.back()->alias_names()) {
        _trie.insert(cmd, item);
    }
    auto protoType = _commands.back()->need_proto_type();
    if (protoType != 0) { // 建立协议-->命令的映射表。
        _app->node_manager().subscribe(protoType, this);
        _protoCommandsTable[protoType] = item;
    }
    return true;
}

[[nodiscard("coroutine function")]]
auto CommandInvoker::setup() -> ::ilias::Task<int>
{
    co_return 0;
}

[[nodiscard("coroutine function")]]
auto CommandInvoker::teardown() -> ::ilias::Task<int>
{
    co_return 0;
}

auto CommandInvoker::name() -> const char *
{
    return "CommandInvoker";
}

auto CommandInvoker::get_subscribes() -> std::vector<int>
{
    return {};
}

[[nodiscard("coroutine function")]]
auto CommandInvoker::handle_event(const NekoProto::IProto &event) -> ::ilias::Task<void>
{
    co_await execute(event);
    co_return;
}

auto CommandInvoker::remove_cmd(NodeBase *module) -> void
{
    if (module == nullptr) {
        _commands.clear();
        _modules.clear();
        _trie.clear();
        for (auto &[key, value] : _protoCommandsTable) {
            _app->node_manager().unsubscribe(key, this);
        }
        _protoCommandsTable.clear();
        SPDLOG_INFO("remove all commands");
        return;
    }
    SPDLOG_INFO("remove commands for module {}<{}>", module->name(), (void *)module);
    auto itemRange = _modules.equal_range(module->name());
    for (auto item = itemRange.first; item != itemRange.second; ++item) {
        const auto &command = *item->second;
        _trie.remove(command->name());
        for (const auto &cmd : command->alias_names()) {
            _trie.remove(cmd);
        }
        _app->node_manager().unsubscribe(command->need_proto_type(), this);
        _protoCommandsTable.erase(command->need_proto_type());
        _commands.erase(item->second);
    }
    _modules.erase(itemRange.first, itemRange.second);
}

auto CommandInvoker::remove_cmd(std::string_view cmd) -> void
{
    auto item = _trie.search(cmd);
    if (item) {
        const auto &command = *(*item);
        _trie.remove(command->name());
        for (const auto &cm : command->alias_names()) {
            _trie.remove(cm);
        }
        _app->node_manager().unsubscribe(command->need_proto_type(), this);
        _protoCommandsTable.erase(command->need_proto_type());
        _commands.erase(*item);
        SPDLOG_INFO("remove command \"{}\"", cmd);
    }
}

auto CommandInvoker::execute(std::span<char> cmd) -> Task<std::string>
{
    co_return co_await execute(_split(cmd));
}

auto CommandInvoker::execute(const std::vector<const char *> &cmdline) -> Task<std::string>
{
    if (cmdline.empty()) {
        co_return "no command";
    }
    const auto *cmd  = cmdline.front();
    auto        item = _trie.search(cmd);
    if (item) {
        (*(*item))->parser_options(cmdline);
        co_return co_await (*(*item))->execute();
    }
    SPDLOG_ERROR("command \"{}\" not found", cmd);
    co_return "command not found";
}

auto CommandInvoker::execute(const NekoProto::IProto &proto) -> Task<std::string>
{
    if (proto == nullptr) {
        SPDLOG_ERROR("command invoker execute a null proto!");
        co_return "proto is empty";
    }
    int  type = proto.type();
    auto item = _protoCommandsTable.find(type);
    if (item != _protoCommandsTable.end()) {
        auto commandItem = item->second;
        (*commandItem)->set_options(proto);
        co_return co_await (*commandItem)->execute();
    }
    co_return "command not found";
}

auto CommandInvoker::show_version([[maybe_unused]] const ArgsType    &args,
                                  [[maybe_unused]] const OptionsType &options) -> Task<std::string>
{
    auto result = fmt::format("{} {}", IApp::app_name(), IApp::app_version());
    fprintf(stdout, "%s\n", result.c_str());
    fflush(stdout);
    co_return result;
}

auto CommandInvoker::show_help([[maybe_unused]] const ArgsType    &args,
                               [[maybe_unused]] const OptionsType &options) -> Task<std::string>
{
    std::string usage = fmt::format("Usage: ${} [command] [args] [options]\n", IApp::app_name());
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
            usage += fmt::format("{}:\n", module);
        }
        const auto &command = (*item->second);
        auto        help    = command->help();
        usage += fmt::format("{}{}", help,
                             ((help.size() > 2 && help[help.size() - 2] != '\n') ? "\n" : ""));
    }
    fprintf(stdout, "%s", usage.c_str());
    ::fflush(stdout);
    co_return usage;
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

MKS_END
