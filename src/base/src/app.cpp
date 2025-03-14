#include "mksync/base/app.hpp"

#include <spdlog/sinks/callback_sink.h>
#include <spdlog/spdlog.h>
#include <csignal>

#ifdef _WIN32
    #include <windows.h>
#elif defined(__linux__)

#endif

namespace mks::base
{
    auto IApp::app_name() -> const char *
    {
        return "mksync";
    }

    auto IApp::app_version() -> const char *
    {
        return "0.0.1";
    }
} // namespace mks::base