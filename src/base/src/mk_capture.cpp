#include "mksync/base/mk_capture.hpp"
#ifdef _WIN32
    #include "mksync/base/platform/win_mk_capture.hpp"
#elif defined(__linux__)
    #include "mksync/base/platform/xcb_mk_capture.hpp"
#else
    #error "Unsupported platform"
#endif

#include "mksync/base/app.hpp"

#include <spdlog/spdlog.h>

namespace mks::base
{
    using ::ilias::Task;

    class MKS_BASE_API CaptureCommand : public Command {
    public:
        enum Operation
        {
            eNone,
            eStart,
            eStop
        };

    public:
        CaptureCommand(MKCapture *capture) : _capture(capture) {}
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
        MKCapture *_capture;
        Operation  _operation = eNone;
    };

    void CaptureCommand::execute()
    {
        switch (_operation) {
        case eStart:
            _capture->start_capture({}, {});
            break;
        case eStop:
            _capture->stop_capture({}, {});
            break;
        default:
            SPDLOG_ERROR("Unknown operation");
            break;
        }
        _operation = eNone;
    }

    void CaptureCommand::undo()
    {
        SPDLOG_ERROR("Undo not supported");
    }

    auto CaptureCommand::description([[maybe_unused]] std::string_view option) const -> std::string
    {
        if (!option.empty()) {
            SPDLOG_WARN("Unknow option {}!", option);
            return std::string("Unknow option ") + std::string(option);
        }
        return "change keyboard/mouse capture mode";
    }

    auto CaptureCommand::options() const -> std::vector<std::string>
    {
        return {};
    }

    auto CaptureCommand::help() const -> std::string
    {
        return "capture <start/stop>\n       change keyboard/mouse capture mode";
    }

    auto CaptureCommand::name() const -> std::string_view
    {
        return "capture";
    }

    auto CaptureCommand::alias_names() const -> std::vector<std::string_view>
    {
        return {};
    }

    void CaptureCommand::set_option(std::string_view                  option,
                                    [[maybe_unused]] std::string_view value)
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

    void CaptureCommand::set_options([[maybe_unused]] const NekoProto::IProto &proto)
    {
        SPDLOG_ERROR("Not implemented proto parameter");
    }

    auto CaptureCommand::get_option([[maybe_unused]] std::string_view option) const -> std::string
    {
        return "";
    }

    auto CaptureCommand::get_options() const -> NekoProto::IProto
    {
        // SPDLOG_ERROR("Not implemented proto parameter");
        return NekoProto::IProto();
    }

    auto MKCapture::start() -> Task<int>
    {
        _isEnable = true;
        co_return 0;
    }

    auto MKCapture::stop() -> Task<int>
    {
        _isEnable = false;
        co_return co_await stop_capture();
    }

    auto MKCapture::start_capture([[maybe_unused]] const CommandInvoker::ArgsType    &args,
                                  [[maybe_unused]] const CommandInvoker::OptionsType &options)
        -> std::string
    {
        if (!_isEnable) {
            SPDLOG_WARN("capture is not enable");
            return "";
        }
        start_capture().wait();
        return "";
    }

    auto MKCapture::stop_capture([[maybe_unused]] const CommandInvoker::ArgsType    &args,
                                 [[maybe_unused]] const CommandInvoker::OptionsType &options)
        -> std::string
    {
        if (!_isEnable) {
            return "";
        }
        stop_capture().wait();
        return "";
    }

    auto MKCapture::make([[maybe_unused]] App &app)
        -> std::unique_ptr<MKCapture, void (*)(NodeBase *)>
    {
        std::unique_ptr<MKCapture, void (*)(NodeBase *)> capture =
#ifdef _WIN32
            {new WinMKCapture(app.get_io_context()),
             [](NodeBase *ptr) { delete static_cast<WinMKCapture *>(ptr); }};
#else
            {new XcbMKCapture(app), [](NodeBase *ptr) { delete static_cast<XcbMKCapture *>(ptr); }};
#endif
        auto commandInstaller = app.command_installer(capture->name());
        // 注册开始捕获键鼠事件命令
        commandInstaller(std::make_unique<CaptureCommand>(capture.get()));
        return capture;
    }

} // namespace mks::base