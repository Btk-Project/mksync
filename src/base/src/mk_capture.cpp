#include "mksync/base/mk_capture.hpp"
#ifdef _WIN32
    #include "mksync/base/platform/win_mk_capture.hpp"
#elif defined(__linux__)
    #include "mksync/base/platform/xcb_mk_capture.hpp"
#else
    #error "Unsupported platform"
#endif

namespace mks::base
{
    auto MKCapture::make([[maybe_unused]] App &app) -> std::unique_ptr<MKCapture>
    {
#ifdef _WIN32
        return std::make_unique<WinMKCapture>();
#else
        return std::make_unique<XcbMKCapture>(app);
#endif
    }

} // namespace mks::base