#include "mksync/core/mk_sender.hpp"
#ifdef _WIN32
    #include "mksync/core/platform/win_mk_sender.hpp"
#elif defined(__linux__)
    #include "mksync/core/platform/xcb_mk_sender.hpp"
#else
    #error "Unsupported platform"
#endif

#include "mksync/core/app.hpp"
#include "mksync/proto/proto.hpp"

#include <spdlog/spdlog.h>

namespace mks::base
{
    using ::ilias::Task;

    class MKS_CORE_API MKSenderCommand : public Command {
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
        auto need_proto_type() const -> int override;

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
        auto results = _options.parse(int(args.size()), args.data());
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
        ILIAS_ASSERT(proto.type() == NekoProto::ProtoFactory::protoType<SenderControl>());
        const auto *senderControl = proto.cast<SenderControl>();
        switch (senderControl->cmd) {
        case SenderControl::eStart:
            _operation = eStart;
            break;
        case SenderControl::eStop:
            _operation = eStop;
            break;
        default:
            SPDLOG_ERROR("Unknown operation");
            break;
        }
    }

    auto MKSenderCommand::need_proto_type() const -> int
    {
        return NekoProto::ProtoFactory::protoType<SenderControl>();
    }

    MKSender::MKSender(IApp *app) : _app(app) {}

    MKSender::~MKSender() {}

    auto MKSender::setup() -> Task<int>
    {
        auto commandInstaller = _app->command_installer(this);
        commandInstaller(std::make_unique<MKSenderCommand>(this));
        SPDLOG_INFO("node {}<{}> setup", name(), (void *)this);
        co_return 0;
    }

    auto MKSender::teardown() -> Task<int>
    {
        _app->command_uninstaller(this);
        SPDLOG_INFO("node {}<{}> teardown", name(), (void *)this);
        co_return co_await stop_sender();
    }

    auto MKSender::get_subscribes() -> std::vector<int>
    {
        return {NekoProto::ProtoFactory::protoType<AppStatusChanged>()};
    }

    auto MKSender::handle_event(const NekoProto::IProto &event) -> ::ilias::Task<void>
    {
        ILIAS_ASSERT(event != nullptr);
        if (event.type() == NekoProto::ProtoFactory::protoType<AppStatusChanged>()) {
            ILIAS_ASSERT(event.cast<AppStatusChanged>() != nullptr);
            const auto *appstatuschanged = event.cast<AppStatusChanged>();
            if (appstatuschanged->status == AppStatusChanged::eStarted &&
                appstatuschanged->mode == AppStatusChanged::eClient) {
                _app->node_manager().subscribe(
                    {NekoProto::ProtoFactory::protoType<MouseMotionEventConversion>(),
                     NekoProto::ProtoFactory::protoType<MouseButtonEvent>(),
                     NekoProto::ProtoFactory::protoType<MouseWheelEvent>(),
                     NekoProto::ProtoFactory::protoType<KeyboardEvent>()},
                    this);
            }
            else if (appstatuschanged->status == AppStatusChanged::eStopped &&
                     appstatuschanged->mode == AppStatusChanged::eClient) {
                _app->node_manager().unsubscribe(
                    {NekoProto::ProtoFactory::protoType<MouseMotionEventConversion>(),
                     NekoProto::ProtoFactory::protoType<MouseButtonEvent>(),
                     NekoProto::ProtoFactory::protoType<MouseWheelEvent>(),
                     NekoProto::ProtoFactory::protoType<KeyboardEvent>()},
                    this);
            }
        }
        co_return;
    }

    auto MKSender::make([[maybe_unused]] IApp *app)
        -> std::unique_ptr<MKSender, void (*)(NodeBase *)>
    {
        std::unique_ptr<MKSender, void (*)(NodeBase *)> sender =
#ifdef _WIN32
            {new WinMKSender(app), [](NodeBase *ptr) { delete static_cast<WinMKSender *>(ptr); }};
#else
            {new XcbMKSender(app), [](NodeBase *ptr) { delete static_cast<XcbMKSender *>(ptr); }};
#endif
        return sender;
    }
} // namespace mks::base