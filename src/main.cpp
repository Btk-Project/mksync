#include "preinclude.hpp"
#include <ilias/platform.hpp>
#include <ilias/signal.hpp>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "app/server.hpp"
#include "app/client.hpp"
#include "config/app_config.hpp"
#include "config/arg_config.hpp"
#include "core.hpp"
#include "platform/platform.hpp"
#include <filesystem>
#include <stacktrace>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <print>
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
        return std::filesystem::path {stateHome} / "mksync" / "mksync.log";
    }
    if (const auto *home = std::getenv("HOME"); home && *home) {
        return std::filesystem::path {home} / ".local" / "state" / "mksync" / "mksync.log";
    }
    return "mksync.log";
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

            auto sinks = std::vector<spdlog::sink_ptr> {
                std::make_shared<spdlog::sinks::stderr_color_sink_mt>(),
                std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath.string(), true),
            };
            auto logger = std::make_shared<spdlog::logger>(
                "mksync",
                sinks.begin(),
                sinks.end()
            );
            logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%s:%#] %v");
            logger->flush_on(spdlog::level::info);
            spdlog::set_default_logger(std::move(logger));
            SPDLOG_INFO("Logging to {}", logPath.string());
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

static auto runPlatformCheck() -> mks::IoTask<void>
{
    try {
        auto platform = mks::Platform::create();
        if (!platform) {
            co_return mks::Err(std::make_error_code(std::errc::operation_not_supported));
        }

        const auto screens = platform->screens();
        SPDLOG_INFO("Platform reported {} screen(s)", screens.size());
        for (auto index = 0U; index < screens.size(); ++index) {
            SPDLOG_INFO("Screen {}: {}", index, screens[index]);
        }

        auto capture = platform->createCapture();
        if (!capture) {
            SPDLOG_ERROR("Current platform does not provide input capture");
            co_return mks::Err(std::make_error_code(std::errc::operation_not_supported));
        }
        ILIAS_CO_TRYV(co_await capture->initialize());
        co_await capture->shutdown();
        SPDLOG_INFO("Input capture initialized and shut down successfully");

        auto injector = platform->createInjector();
        if (!injector) {
            SPDLOG_ERROR("Current platform does not provide input injection");
            co_return mks::Err(std::make_error_code(std::errc::operation_not_supported));
        }
        ILIAS_CO_TRYV(co_await injector->initialize());
        co_await injector->shutdown();
        SPDLOG_INFO("Input injector initialized and shut down successfully");

        co_return {};
    }
    catch (const std::exception &err) {
        SPDLOG_ERROR("Platform check failed: {}", err.what());
        co_return mks::Err(std::make_error_code(std::errc::operation_not_supported));
    }
}

static auto makeParserConfig() -> NekoProto::argparser::ArgParserConfig
{
    auto config        = NekoProto::argparser::ArgParserConfig{};
    config.programName = "mksync";
    config.description = "mksync - simple mouse keyboard tools";
    config.version     = MKS_VERSION_STR;
    config.configIo.emplace();
    config.configIo->enableFormat("toml");
    // config.configIo->enableFormat("json");
    return config;
}

static auto printHelp(int argc, char **argv, NekoProto::argparser::ArgParserConfig config) -> void
{
    std::cout << NekoProto::argparser::format_help<mks::CliCommands>(argc, argv, config);
}

using CliCommand = std::variant<mks::ServerCommand, mks::ClientCommand, mks::CheckPlatformCommand>;

static auto parseCliCommand(int argc, char **argv) -> std::optional<CliCommand>
{
    auto parserConfig = makeParserConfig();
    auto parsed       = NekoProto::argparser::parser<mks::CliCommands>(argc, argv, parserConfig);
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

    if (std::holds_alternative<mks::CheckPlatformCommand>(*command)) {
        auto checked = co_await runPlatformCheck();
        if (!checked) {
            SPDLOG_ERROR("Platform check failed: {}", checked.error().message());
            std::exit(EXIT_FAILURE);
        }
        co_return;
    }

    if (const auto *serverCommand = std::get_if<mks::ServerCommand>(&*command)) {
        auto endpoint = ilias::IPEndpoint::fromString(serverCommand->endpoint);
        if (!endpoint) {
            SPDLOG_ERROR("Invalid endpoint: {}", serverCommand->endpoint);
            co_return;
        }
        auto loaded = loadAppConfig(serverCommand->configPath);
        if (!loaded) {
            co_return;
        }
        mks::Server server{*endpoint, loaded->app, loaded->path};
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
        auto endpoint = ilias::IPEndpoint::fromString(clientCommand->endpoint);
        if (!endpoint) {
            SPDLOG_ERROR("Invalid endpoint: {}", clientCommand->endpoint);
            co_return;
        }
        auto loaded = loadAppConfig(clientCommand->configPath);
        if (!loaded) {
            co_return;
        }
        mks::Client client{*endpoint, loaded->app};
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
