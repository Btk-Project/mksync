#include "mksync/base/mk_sender.hpp"
#ifdef _WIN32
    #include "mksync/base/platform/win_mk_sender.hpp"
#elif defined(__linux__)
    #include "mksync/base/platform/xcb_mk_sender.hpp"
#else
    #error "Unsupported platform"
#endif
namespace mks::base
{
    auto MKSender::make([[maybe_unused]] App &app) -> std::unique_ptr<MKSender>
    {
#ifdef _WIN32
        return std::make_unique<WinMKSender>();
#else
        return std::make_unique<XcbMKSender>(app);
#endif
    }
} // namespace mks::base