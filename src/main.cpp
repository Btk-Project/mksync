#include "preinclude.hpp"
#include <argparse/argparse.hpp>
#include <ilias/platform.hpp>
#include <ilias/signal.hpp>
#include "app/server.hpp"
#include "core.hpp"
#include <stacktrace>
#include <csignal>
#include <print>

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

void ilias_main(int argc, char **argv) {
    argparse::ArgumentParser program {"mksync"};
    bool hasError = false;

    program.add_description("mksync - simple mouse keyboard tools");

    auto &mode_group = program.add_mutually_exclusive_group(true);

    mode_group.add_argument("--listen")
        .help("listen on address, e.g. 127.0.0.1:1145")
        .metavar("HOST:PORT");

    mode_group.add_argument("--connect")
        .help("connect to address, e.g. 127.0.0.1:1145")
        .metavar("HOST:PORT");

    try {
        program.parse_args(argc, argv);
    }
    catch (const std::exception &err) {
        std::cerr << err.what() << "\n\n";
        std::cerr << program;
        hasError = true; // MAKE THE LSP HAPPY :(
    }
    if (hasError) {
        co_return;
    }

    // Server mode
    if (auto value = program.present<std::string>("--listen")) {
        auto endpoint = ilias::IPEndpoint::fromString(*value);
        if (!endpoint) {
            SPDLOG_ERROR("Invalid endpoint: {}", *value);
            co_return;
        }
        mks::Server server {*endpoint};
        auto [err, ctrlc] = co_await ilias::whenAny(
            server.run(),
            ilias::signal::ctrlC()
        );
        if (ctrlc) {
            SPDLOG_WARN("Ctrl-C received, shutting down...");
        }
        if (err) {
            SPDLOG_ERROR("Server error: {}", err->error().message());
        }
    } 
    else if (auto value = program.present<std::string>("--connect")) {
        SPDLOG_WARN("connect mode not implemented");
    }
    co_return;
}