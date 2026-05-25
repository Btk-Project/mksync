#include <ilias/platform.hpp>
#include <stacktrace>
#include <csignal>
#include <print>
#include "core.hpp"
#include "platform/platform.hpp"

static void crashHandler() {
    std::println("Crashed");
    std::println("Stacktrace:");
    std::println("{}", std::stacktrace::current());
}

static void crashHandlerSignal(int) {
    crashHandler();
}

static int _init = []() {
    std::signal(SIGSEGV, crashHandlerSignal);
    std::signal(SIGABRT, crashHandlerSignal);

#if defined(_WIN32)
    ::SetUnhandledExceptionFilter([](EXCEPTION_POINTERS*) -> LONG {
        crashHandler();
        return EXCEPTION_EXECUTE_HANDLER;
    });
#endif

    return 0;
}();

void ilias_main() {
    auto platform = mks::Platform::create();
    auto capture = platform->createCapture();
    if (auto res = co_await capture->initialize(); !res) {
        std::println("Failed to initialize input capture");
        co_return;
    }
    while (true) {
        auto event = co_await capture->nextEvent();
    }
    co_return;
}