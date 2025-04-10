#include "mksync/base/app.hpp"

#include <csignal>

#include <spdlog/sinks/callback_sink.h>
#include <spdlog/spdlog.h>

#ifdef _WIN32
    #include <windows.h>
#elif defined(__linux__)

#endif

MKS_BEGIN

auto IApp::app_name() -> const char *
{
    return "mksync";
}

auto IApp::app_version() -> const char *
{
    return MKS_VERSION;
}

MKS_END