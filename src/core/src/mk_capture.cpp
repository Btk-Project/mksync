#include "mksync/core/mk_capture.hpp"
#ifdef _WIN32
    #include "mksync/core/platform/win_mk_capture.hpp"
#elif defined(__linux__)
    #include "mksync/core/platform/xcb_mk_capture.hpp"
#else
    #error "Unsupported platform"
#endif

#include "mksync/core/app.hpp"
#include "mksync/proto/proto.hpp"

#include <spdlog/spdlog.h>

namespace mks::base
{
    using ::ilias::Task;

    class MKS_CORE_API CaptureCommand : public Command {
    public:
        enum Operation
        {
            eNone,
            eStart,
            eStop
        };

    public:
        CaptureCommand(MKCapture *capture);
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
        MKCapture       *_capture;
        Operation        _operation = eNone;
        cxxopts::Options _options;
    };

    CaptureCommand::CaptureCommand(MKCapture *capture) : _capture(capture), _options("capture")
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
        _options.custom_help(fmt::format("{}{}{}{} <start/stop> keyboard/mouse capture mode.",
                                         this->name(), ret.empty() ? "" : "(", ret,
                                         ret.empty() ? "" : ")"));
        _options.allow_unrecognised_options();
    }

    auto CaptureCommand::execute() -> Task<void>
    {
        switch (_operation) {
        case eStart:
            co_await _capture->start_capture();
            break;
        case eStop:
            co_await _capture->stop_capture();
            break;
        default:
            SPDLOG_ERROR("Unknown operation");
            break;
        }
        _operation = eNone;
        co_return;
    }

    void CaptureCommand::parser_options(const std::vector<const char *> &args)
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

    auto CaptureCommand::help() const -> std::string
    {
        return _options.help({}, false);
    }

    auto CaptureCommand::name() const -> std::string_view
    {
        return _options.program();
    }

    auto CaptureCommand::alias_names() const -> std::vector<std::string_view>
    {
        return {};
    }

    void CaptureCommand::set_option(std::string_view                  option,
                                    [[maybe_unused]] std::string_view value)
    {
        if (!option.empty()) {
            SPDLOG_ERROR("Unknow option {} = {} !", option, value);
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

    void CaptureCommand::set_options([[maybe_unused]] const NekoProto::IProto &proto)
    {
        ILIAS_ASSERT(proto.type() == NekoProto::ProtoFactory::protoType<CaptureControl>());
        const auto *captureControl = proto.cast<CaptureControl>();
        switch (captureControl->cmd) {
        case CaptureControl::eStart:
            _operation = eStart;
            break;
        case CaptureControl::eStop:
            _operation = eStop;
            break;
        default:
            SPDLOG_ERROR("Unknown operation");
        }
    }

    auto CaptureCommand::get_option([[maybe_unused]] std::string_view option) const -> std::string
    {
        return "";
    }

    auto CaptureCommand::get_options() const -> NekoProto::IProto
    {
        // SPDLOG_ERROR("Not implemented proto parameter");
        return CaptureControl::emplaceProto();
    }

    auto MKCapture::enable() -> Task<int>
    {
        co_return 0;
    }

    auto MKCapture::disable() -> Task<int>
    {
        co_return co_await stop_capture();
    }

    auto MKCapture::make([[maybe_unused]] IApp *app)
        -> std::unique_ptr<MKCapture, void (*)(NodeBase *)>
    {
        std::unique_ptr<MKCapture, void (*)(NodeBase *)> capture =
#ifdef _WIN32
            {new WinMKCapture(app->get_io_context()),
             [](NodeBase *ptr) { delete static_cast<WinMKCapture *>(ptr); }};
#else
            {new XcbMKCapture(app), [](NodeBase *ptr) { delete static_cast<XcbMKCapture *>(ptr); }};
#endif
        auto commandInstaller = app->command_installer(capture->name());
        // 注册开始捕获键鼠事件命令
        commandInstaller(std::make_unique<CaptureCommand>(capture.get()));
        return capture;
    }

} // namespace mks::base