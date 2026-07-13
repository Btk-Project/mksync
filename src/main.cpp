#include "app/client.hpp"
#include "app/server.hpp"
#include "config/app_config.hpp"
#include "config/arg_config.hpp"
#include "core.hpp"
#include "platform/backend.hpp"
#include "platform/platform.hpp"
#include "preinclude.hpp"
#include <cctype>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <ilias/platform.hpp>
#include <ilias/signal.hpp>
#include <iostream>
#include <optional>
#include <print>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <stacktrace>
#include <string>
#include <string_view>
#include <system_error>
#include <variant>

static void crashHandler()
{
    std::println("Crashed");
    std::println("Stacktrace:");
    std::println("{}", std::stacktrace::current());
    if (auto logger = spdlog::default_logger()) {
        logger->flush();
    }
}

static auto logFilePath() -> std::filesystem::path
{
    if (const auto *stateHome = std::getenv("XDG_STATE_HOME"); stateHome && *stateHome) {
        return std::filesystem::path{stateHome} / "mksync" / "mksync.log";
    }
    if (const auto *home = std::getenv("HOME"); home && *home) {
        return std::filesystem::path{home} / ".local" / "state" / "mksync" / "mksync.log";
    }
    return "mksync.log";
}

static auto parseLogLevel(std::string_view text) -> spdlog::level::level_enum
{
    auto normalized = std::string{};
    normalized.reserve(text.size());
    for (auto ch : text) {
        normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }

    if (normalized == "trace") {
        return spdlog::level::trace;
    }
    if (normalized == "debug") {
        return spdlog::level::debug;
    }
    if (normalized == "warn" || normalized == "warning") {
        return spdlog::level::warn;
    }
    if (normalized == "err" || normalized == "error") {
        return spdlog::level::err;
    }
    if (normalized == "critical") {
        return spdlog::level::critical;
    }
    if (normalized == "off") {
        return spdlog::level::off;
    }
    return spdlog::level::info;
}

static void initializeLogging()
{
    static std::once_flag once;
    std::call_once(once, []() {
        try {
            const auto logPath = logFilePath();
            if (const auto parent = logPath.parent_path(); !parent.empty()) {
                std::filesystem::create_directories(parent);
            }

            auto sinks = std::vector<spdlog::sink_ptr>{
                std::make_shared<spdlog::sinks::stderr_color_sink_mt>(),
                std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath.string(), true),
            };
            auto logger = std::make_shared<spdlog::logger>("mksync", sinks.begin(), sinks.end());
            logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%s:%#] %v");
            logger->flush_on(spdlog::level::info);
            spdlog::set_default_logger(std::move(logger));
        }
        catch (const std::exception &err) {
            std::cerr << "Failed to initialize file logging: " << err.what() << '\n';
        }
    });
}

static void crashHandlerSignal(int signum)
{
    static volatile std::sig_atomic_t handlingCrash = 0;
    if (handlingCrash) {
        std::_Exit(128 + signum);
    }
    handlingCrash = 1;

    crashHandler();
    std::signal(signum, SIG_DFL);
    std::raise(signum);
    std::_Exit(128 + signum);
}

static int _init = []() {
    std::signal(SIGSEGV, crashHandlerSignal);
    std::signal(SIGABRT, crashHandlerSignal);

#if defined(_WIN32)
    ::SetUnhandledExceptionFilter([](EXCEPTION_POINTERS *) -> LONG {
        crashHandler();
        return EXCEPTION_EXECUTE_HANDLER;
    });
#endif

    return 0;
}();

static auto checkStatus(bool supported) -> std::string_view
{
    return supported ? "yes" : "no";
}

static auto printBackendCheck(const mks::BackendDescriptor &descriptor,
                              const mks::BackendCheck      &check) -> void
{
    std::println("{}\t{}", descriptor.name, descriptor.displayName);
    std::println("  available: {} ({})", checkStatus(check.available), check.detail);
    std::println("  screens:   {} ({})", checkStatus(check.screens.supported),
                 check.screens.detail);
    std::println("  capture:   {} ({})", checkStatus(check.capture.supported),
                 check.capture.detail);
    std::println("  injection: {} ({})", checkStatus(check.injection.supported),
                 check.injection.detail);
}

static auto listBackends(bool checked) -> mks::Task<void>
{
    if (!checked) {
        for (const auto &descriptor : mks::registeredBackends()) {
            std::println("{}\t{}", descriptor.name, descriptor.displayName);
        }
        co_return;
    }

    for (const auto &backend : co_await mks::checkBackends()) {
        printBackendCheck(backend.descriptor, backend.check);
    }
}

static auto runPlatformCheck(std::string_view requestedBackend) -> mks::IoTask<void>
{
    const auto requirements = mks::BackendRequirement::Screens | mks::BackendRequirement::Capture |
                              mks::BackendRequirement::Injection;
    if (!requestedBackend.empty() && requestedBackend != "auto") {
        for (const auto &descriptor : mks::registeredBackends()) {
            if (descriptor.name != requestedBackend) {
                continue;
            }
            const auto check = co_await mks::checkBackend(descriptor.name);
            printBackendCheck(descriptor, check);
            co_return check.supports(requirements)
                ? mks::IoResult<void>{}
                : mks::Err(std::make_error_code(std::errc::operation_not_supported));
        }
        SPDLOG_ERROR("Unknown platform backend '{}'", requestedBackend);
        co_return mks::Err(std::make_error_code(std::errc::no_such_device));
    }

    for (const auto &backend : co_await mks::checkBackends()) {
        printBackendCheck(backend.descriptor, backend.check);
        if (backend.check.supports(requirements)) {
            SPDLOG_INFO("Automatic platform check selected '{}'", backend.descriptor.name);
            co_return {};
        }
    }
    co_return mks::Err(std::make_error_code(std::errc::operation_not_supported));
}

static auto selectRuntimeBackend(std::string_view name, mks::BackendRequirement requirement)
    -> mks::IoTask<mks::PlatformSelection>
{
    co_return co_await mks::selectBackend(name, mks::BackendRequirement::Screens | requirement);
}

static auto printHelp(int argc, char **argv, NekoProto::argparser::ArgParserConfig config) -> void
{
    std::cout << NekoProto::argparser::format_help<mks::CliCommands>(argc, argv, config);
}

static auto parseCliCommand(int argc, char **argv) -> std::optional<mks::CliCommand>
{
    auto parserConfig = mks::makeCliParserConfig();
    auto parsed       = mks::parseCliArguments(argc, const_cast<const char *const *>(argv));
    if (!parsed) {
        if (parsed.error() ==
            make_error_code(NekoProto::argparser::ArgParserError::HelpRequested)) {
            printHelp(argc, argv, parserConfig);
            return std::nullopt;
        }
        if (parsed.error() ==
            make_error_code(NekoProto::argparser::ArgParserError::VersionRequested)) {
            std::cout << NekoProto::argparser::format_version(parserConfig);
            return std::nullopt;
        }

        std::cerr << parsed.error().message() << "\n\n";
        printHelp(argc, argv, parserConfig);
        return std::nullopt;
    }

    return std::move(*parsed);
}

struct LoadedAppConfig {
    std::filesystem::path path;
    mks::AppConfig        app;
};

static auto loadAppConfig(const std::string &configPathText) -> std::optional<LoadedAppConfig>
{
    auto configPath   = std::filesystem::path{configPathText};
    auto configResult = mks::loadOrCreateConfig(configPath);
    if (!configResult) {
        SPDLOG_ERROR("Failed to load config {}: {}", configPath.string(),
                     configResult.error().message());
        return std::nullopt;
    }

    SPDLOG_INFO("Using config {} with machineId {}", configPath.string(), configResult->machineId);
    return LoadedAppConfig{.path = std::move(configPath), .app = std::move(*configResult)};
}

void ilias_main(int argc, char **argv)
{
    initializeLogging();

    auto command = parseCliCommand(argc, argv);
    if (!command) {
        co_return;
    }

    if (const auto *backendCommand = std::get_if<mks::BackendCommand>(&*command)) {
        co_await listBackends(backendCommand->checked);
        co_return;
    }

    if (const auto *checkCommand = std::get_if<mks::CheckPlatformCommand>(&*command)) {
        spdlog::set_level(parseLogLevel("trace"));
        auto checked = co_await runPlatformCheck(checkCommand->backend);
        if (!checked) {
            SPDLOG_ERROR("Platform check failed: {}", checked.error().message());
            std::exit(EXIT_FAILURE);
        }
        co_return;
    }

    if (const auto *serverCommand = std::get_if<mks::ServerCommand>(&*command)) {
        spdlog::set_level(parseLogLevel(serverCommand->common.logLevel));
        auto endpoint = ilias::IPEndpoint::fromString(serverCommand->endpoint);
        if (!endpoint) {
            SPDLOG_ERROR("Invalid endpoint: {}", serverCommand->endpoint);
            co_return;
        }
        auto loaded = loadAppConfig(serverCommand->common.configPath);
        if (!loaded) {
            co_return;
        }
        auto selected = co_await selectRuntimeBackend(serverCommand->common.backend,
                                                      mks::BackendRequirement::Capture);
        if (!selected) {
            SPDLOG_ERROR("Failed to select a platform backend for server mode: {}",
                         selected.error().message());
            co_return;
        }
        mks::Server server{std::move(selected->platform), *endpoint, loaded->app, loaded->path};
        auto [serverResult, ctrlc] = co_await ilias::whenAny(server.run(), ilias::signal::ctrlC());
        if (ctrlc) {
            SPDLOG_WARN("Ctrl-C received, shutting down...");
        }
        if (serverResult && !*serverResult) {
            SPDLOG_ERROR("Server error: {}", serverResult->error().message());
        }
        else if (serverResult) {
            SPDLOG_INFO("Server stopped");
        }
    }
    else if (const auto *clientCommand = std::get_if<mks::ClientCommand>(&*command)) {
        spdlog::set_level(parseLogLevel(clientCommand->common.logLevel));
        auto endpoint = ilias::IPEndpoint::fromString(clientCommand->endpoint);
        if (!endpoint) {
            SPDLOG_ERROR("Invalid endpoint: {}", clientCommand->endpoint);
            co_return;
        }
        auto loaded = loadAppConfig(clientCommand->common.configPath);
        if (!loaded) {
            co_return;
        }
        auto selected = co_await selectRuntimeBackend(clientCommand->common.backend,
                                                      mks::BackendRequirement::Injection);
        if (!selected) {
            SPDLOG_ERROR("Failed to select a platform backend for client mode: {}",
                         selected.error().message());
            co_return;
        }
        mks::Client client{std::move(selected->platform), *endpoint, loaded->app};
        auto [clientResult, ctrlc] = co_await ilias::whenAny(client.run(), ilias::signal::ctrlC());
        if (ctrlc) {
            SPDLOG_WARN("Ctrl-C received, shutting down...");
        }
        if (clientResult && !*clientResult) {
            SPDLOG_ERROR("Client error: {}", clientResult->error().message());
        }
        else if (clientResult) {
            SPDLOG_INFO("Client stopped");
        }
    }
    co_return;
}
