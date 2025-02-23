#include "mksync/base/mk_capture.hpp"
#ifdef _WIN32
    #include "mksync/base/platform/win_mk_capture.hpp"
#endif

namespace mks::base
{
    auto MKCapture::make() -> std::unique_ptr<MKCapture>
    {
#ifdef _WIN32
        return std::make_unique<WinMKCapture>();
#else
        return nullptr;
#endif
    }

} // namespace mks::base