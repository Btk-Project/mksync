#include "mksync/base/mk_sender.hpp"
#ifdef _WIN32
    #include "mksync/base/platform/win_mk_sender.hpp"
#elif defined(__linux__)
    #include "mksync/base/platform/xcb_mk_sender.hpp"
#else
    #error "Unsupported platform"
#endif

#include "mksync/base/app.hpp"

#include <spdlog/spdlog.h>

namespace mks::base
{
    using ::ilias::Task;

    class MKS_BASE_API MKSenderCommand : public Command {
    public:
        enum Operation
        {
            eNone,
            eStart,
            eStop
        };

    public:
        MKSenderCommand(MKSender *sender);
        auto execute() -> Task<void> override;
        auto help() const -> std::string override;
        auto name() const -> std::string_view override;
        auto alias_names() const -> std::vector<std::string_view> override;
        void set_option(std::string_view option, std::string_view value) override;
        void set_options(const NekoProto::IProto &proto) override;
        void parser_options(const std::vector<const char *> &args) override;
        auto get_option(std::string_view option) const -> std::string override;
        auto get_options() const -> NekoProto::IProto override;

    private:
        MKSender        *_sender;
        Operation        _operation = eNone;
        cxxopts::Options _options;
    };

    MKSenderCommand::MKSenderCommand(MKSender *sender) : _sender(sender), _options("sender")
    {
        std::string ret;
        for (auto alias : alias_names()) {
            ret += alias;
            ret += ", ";
        }
        if (!ret.empty()) {
            ret.pop_back();
            ret.pop_back();
        }
        _options.custom_help(fmt::format("{}{}{}{} <start/stop> keyboard/mouse sender mode.",
                                         this->name(), ret.empty() ? "" : "(", ret,
                                         ret.empty() ? "" : ")"));
        _options.allow_unrecognised_options();
    }

    auto MKSenderCommand::execute() -> Task<void>
    {
        switch (_operation) {
        case eStart:
            co_await _sender->start_sender();
            break;
        case eStop:
            co_await _sender->stop_sender();
            break;
        default:
            SPDLOG_ERROR("Unknown operation");
            break;
        }
        co_return;
    }

    void MKSenderCommand::parser_options(const std::vector<const char *> &args)
    {
        auto results = _options.parse(args.size(), args.data());
        for (const auto &result : results.unmatched()) {
            if (result == "start") {
                _operation = eStart;
            }
            else if (result == "stop") {
                _operation = eStop;
            }
            else {
                SPDLOG_ERROR("Unknown operation {}", result);
            }
        }
        for (const auto &result : results) {
            SPDLOG_ERROR("Unknow option {} = {}", result.key(), result.value());
        }
    }

    auto MKSenderCommand::help() const -> std::string
    {
        return _options.help({}, false);
    }

    auto MKSenderCommand::name() const -> std::string_view
    {
        return _options.program();
    }

    auto MKSenderCommand::alias_names() const -> std::vector<std::string_view>
    {
        return {};
    }

    void MKSenderCommand::set_option(std::string_view option, std::string_view value)
    {
        if (!option.empty()) {
            SPDLOG_ERROR("Unknow option {}={} !", option, value);
            return;
        }
        if (value == "start") {
            _operation = eStart;
        }
        else if (value == "stop") {
            _operation = eStop;
        }
        else {
            SPDLOG_ERROR("Unknow operation {}!", value);
        }
    }

    void MKSenderCommand::set_options([[maybe_unused]] const NekoProto::IProto &proto)
    {
        SPDLOG_ERROR("Not implemented proto parameter");
    }

    auto MKSenderCommand::get_option([[maybe_unused]] std::string_view option) const -> std::string
    {
        return "";
    }

    auto MKSenderCommand::get_options() const -> NekoProto::IProto
    {
        return NekoProto::IProto();
    }

    auto MKSender::start() -> Task<int>
    {
        co_return 0;
    }

    auto MKSender::stop() -> Task<int>
    {
        co_return co_await stop_sender();
    }

    auto MKSender::make([[maybe_unused]] App &app)
        -> std::unique_ptr<MKSender, void (*)(NodeBase *)>
    {
        std::unique_ptr<MKSender, void (*)(NodeBase *)> sender =
#ifdef _WIN32
            {new WinMKSender(app.get_io_context()),
             [](NodeBase *ptr) { delete static_cast<WinMKSender *>(ptr); }};
#else
            {new XcbMKSender(app), [](NodeBase *ptr) { delete static_cast<XcbMKSender *>(ptr); }};
#endif
        auto commandInstaller = app.command_installer(sender->name());
        commandInstaller(std::make_unique<MKSenderCommand>(sender.get()));
        return sender;
    }
} // namespace mks::base