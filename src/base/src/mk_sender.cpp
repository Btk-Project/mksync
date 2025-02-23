#include "mksync/base/mk_sender.hpp"
#ifdef _WIN32
    #include "mksync/base/platform/win_mk_sender.hpp"
#endif
namespace mks::base
{
    auto MKSender::make() -> std::unique_ptr<MKSender>
    {
#ifdef _WIN32
        return std::make_unique<WinMKSender>();
#else
        return nullptr;
#endif
    }
} // namespace mks::base