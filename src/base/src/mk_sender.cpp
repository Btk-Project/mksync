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

    auto MKSender::start() -> ::ilias::Task<int>
    {
        _isEnable = true;
        co_return 0;
    }

    auto MKSender::stop() -> ::ilias::Task<int>
    {
        _isEnable = false;
        co_return co_await stop_sender();
    }

    auto MKSender::start_sender([[maybe_unused]] const CommandParser::ArgsType    &args,
                                [[maybe_unused]] const CommandParser::OptionsType &options)
        -> std::string
    {
        if (!_isEnable) {
            spdlog::warn("{} is not enable", name());
            return "";
        }
        start_sender().wait();
        return "";
    }

    auto MKSender::stop_sender([[maybe_unused]] const CommandParser::ArgsType    &args,
                               [[maybe_unused]] const CommandParser::OptionsType &options)
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
        using CallbackType = std::string (MKSender::*)(const CommandParser::ArgsType &,
                                                       const CommandParser::OptionsType &);
        std::unique_ptr<MKSender, void (*)(NodeBase *)> sender =
#ifdef _WIN32
            {new WinMKSender(app.get_io_context()),
             [](NodeBase *ptr) { delete static_cast<WinMKSender *>(ptr); }};
#else
            {new XcbMKSender(app), [](NodeBase *ptr) { delete static_cast<XcbMKSender *>(ptr); }};
#endif
        auto commandInstaller = app.command_installer(sender->name());
        commandInstaller({{"start_sender"},
                          "start mouse/keyboard sender",
                          std::bind(static_cast<CallbackType>(&MKSender::start_sender),
                                    sender.get(), std::placeholders::_1, std::placeholders::_2)});
        commandInstaller({{"stop_sender"},
                          "stop mouse/keyboard sender",
                          std::bind(static_cast<CallbackType>(&MKSender::stop_sender), sender.get(),
                                    std::placeholders::_1, std::placeholders::_2)});
        return sender;
    }
} // namespace mks::base