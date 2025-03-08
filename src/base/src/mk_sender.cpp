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
        MKSenderCommand(MKSender *sender) : _sender(sender) {}
        void execute() override;
        void undo() override;
        auto description(std::string_view option) const -> std::string override;
        auto options() const -> std::vector<std::string> override;
        auto help() const -> std::string override;
        auto name() const -> std::string_view override;
        auto alias_names() const -> std::vector<std::string_view> override;
        void set_option(std::string_view option, std::string_view value) override;
        void set_options(const NekoProto::IProto &proto) override;
        auto get_option(std::string_view option) const -> std::string override;
        auto get_options() const -> NekoProto::IProto override;

    private:
        MKSender *_sender;
        Operation _operation = eNone;
    };

    void MKSenderCommand::execute()
    {
        switch (_operation) {
        case eStart:
            _sender->start_sender({}, {});
            break;
        case eStop:
            _sender->stop_sender({}, {});
            break;
        default:
            SPDLOG_ERROR("Unknown operation");
            break;
        }
    }

    void MKSenderCommand::undo()
    {
        SPDLOG_ERROR("Undo not supported");
    }

    auto MKSenderCommand::description([[maybe_unused]] std::string_view option) const -> std::string
    {
        if (!option.empty()) {
            SPDLOG_WARN("Unknow option {}!", option);
            return std::string("Unknow option ") + std::string(option);
        }
        return "change keyboard/mouse sender mode";
    }

    auto MKSenderCommand::options() const -> std::vector<std::string>
    {
        return {};
    }

    auto MKSenderCommand::help() const -> std::string
    {
        return "sender <start/stop>\n       change keyboard/mouse sender mode";
    }

    auto MKSenderCommand::name() const -> std::string_view
    {
        return "sender";
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
        _isEnable = true;
        co_return 0;
    }

    auto MKSender::stop() -> Task<int>
    {
        _isEnable = false;
        co_return co_await stop_sender();
    }

    auto MKSender::start_sender([[maybe_unused]] const CommandInvoker::ArgsType    &args,
                                [[maybe_unused]] const CommandInvoker::OptionsType &options)
        -> std::string
    {
        if (!_isEnable) {
            SPDLOG_WARN("{} is not enable", name());
            return "";
        }
        start_sender().wait();
        return "";
    }

    auto MKSender::stop_sender([[maybe_unused]] const CommandInvoker::ArgsType    &args,
                               [[maybe_unused]] const CommandInvoker::OptionsType &options)
        -> std::string
    {
        if (!_isEnable) {
            return "";
        }
        stop_sender().wait();
        return "";
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