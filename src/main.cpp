#include "preinclude.hpp"
#include <argparse/argparse.hpp>
#include <ilias/platform.hpp>
#include <ilias/signal.hpp>
#include "app/server.hpp"
#include "app/client.hpp"
#include "config/app_config.hpp"
#include "core.hpp"
#include <filesystem>
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

    program.add_argument("-c", "--config")
        .help("configuration file path")
        .default_value(std::string {"mksync.json"})
        .metavar("PATH");

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

    auto configPath = std::filesystem::path {program.get<std::string>("--config")};
    auto configResult = mks::loadOrCreateConfig(configPath);
    if (!configResult) {
        SPDLOG_ERROR("Failed to load config {}: {}", configPath.string(), configResult.error().message());
        co_return;
    }
    auto config = std::move(*configResult);
    SPDLOG_INFO("Using config {} with machineId {}", configPath.string(), config.machineId);

    // Server mode
    if (auto value = program.present<std::string>("--listen")) {
        auto endpoint = ilias::IPEndpoint::fromString(*value);
        if (!endpoint) {
            SPDLOG_ERROR("Invalid endpoint: {}", *value);
            co_return;
        }
        mks::Server server {*endpoint, config, configPath};
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
    else if (auto connectValue = program.present<std::string>("--connect")) {
        auto endpoint = ilias::IPEndpoint::fromString(*connectValue);
        if (!endpoint) {
            SPDLOG_ERROR("Invalid endpoint: {}", *connectValue);
            co_return;
        }
        mks::Client client {*endpoint, config};
        auto [err, ctrlc] = co_await ilias::whenAny(
            client.run(),
            ilias::signal::ctrlC()
        );
        if (ctrlc) {
            SPDLOG_WARN("Ctrl-C received, shutting down...");
        }
        if (err) {
            SPDLOG_ERROR("Client error: {}", err->error().message());
        }
    }
    co_return;
}
