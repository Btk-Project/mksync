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

    auto MKCapture::start_capture([[maybe_unused]] const CommandParser::ArgsType    &args,
                                  [[maybe_unused]] const CommandParser::OptionsType &options)
        -> std::string
    {
        if (!_isEnable) {
            SPDLOG_WARN("capture is not enable");
            return "";
        }
        start_capture().wait();
        return "";
    }

    auto MKCapture::stop_capture([[maybe_unused]] const CommandParser::ArgsType    &args,
                                 [[maybe_unused]] const CommandParser::OptionsType &options)
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
        using CallbackType = std::string (MKCapture::*)(const CommandParser::ArgsType &,
                                                        const CommandParser::OptionsType &);
        std::unique_ptr<MKCapture, void (*)(NodeBase *)> capture =
#ifdef _WIN32
            {new WinMKCapture(app.get_io_context()),
             [](NodeBase *ptr) { delete static_cast<WinMKCapture *>(ptr); }};
#else
            {new XcbMKCapture(app), [](NodeBase *ptr) { delete static_cast<XcbMKCapture *>(ptr); }};
#endif
        auto commandInstaller = app.command_installer(capture->name());
        // 注册开始捕获键鼠事件命令
        commandInstaller({
            {"capture", "c"},
            "start keyboard/mouse capture",
            std::bind(static_cast<CallbackType>(&MKCapture::start_capture), capture.get(),
                      std::placeholders::_1, std::placeholders::_2),
            {}
        });
        commandInstaller({{"stopcapture"},
                          "stop keyboard/mouse capture",
                          std::bind(static_cast<CallbackType>(&MKCapture::stop_capture),
                                    capture.get(), std::placeholders::_1, std::placeholders::_2),
                          {}});
        return capture;
    }

} // namespace mks::base