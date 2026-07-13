#pragma once

#include "config/config.hpp"

#include <ilias/io/error.hpp>
#include <nekoproto/argparser/argparser.hpp>

#include <filesystem>
#include <string>
#include <variant>

MKS_BEGIN

struct CommonConfig {
    std::string configPath = "mksync.json";
    std::string logLevel   = "info";
    std::string backend;
};

struct ServerCommand {
    std::string  endpoint;
    CommonConfig common;
};

struct ClientCommand {
    std::string  endpoint;
    CommonConfig common;
};

struct CheckPlatformCommand {
    std::string backend;
};

struct BackendCommand {
    bool list    = false;
    bool checked = false;
};

struct CliCommands {
    ServerCommand        server;
    ClientCommand        client;
    CheckPlatformCommand checkPlatform;
    BackendCommand       backend;
};

using CliCommand = std::variant<ServerCommand, ClientCommand, CheckPlatformCommand, BackendCommand>;

auto makeCliParserConfig() -> NekoProto::argparser::ArgParserConfig;
auto parseCliArguments(int argc, const char *const *argv) -> ilias::IoResult<CliCommand>;
auto importCliCommand(const std::filesystem::path &path) -> ilias::IoResult<CliCommand>;
auto exportCliCommand(const std::filesystem::path &path, const CliCommand &command)
    -> ilias::IoResult<void>;

MKS_END

namespace NekoProto
{

    namespace mksArgparser = argparser;

    template <>
    struct Meta<::mks::ServerCommand, void> {
        constexpr static auto value =
            Object("endpoint",
                   make_tags<mksArgparser::arg_value_name<"HOST:PORT">,
                             mksArgparser::arg_help<"listen endpoint">,
                             mksArgparser::ArgTags{.required = true, .positional = true}>(
                       &::mks::ServerCommand::endpoint),
                   "common", &::mks::ServerCommand::common);
    };

    template <>
    struct Meta<::mks::ClientCommand, void> {
        constexpr static auto value =
            Object("endpoint",
                   make_tags<mksArgparser::arg_value_name<"HOST:PORT">,
                             mksArgparser::arg_help<"server endpoint">,
                             mksArgparser::ArgTags{.required = true, .positional = true}>(
                       &::mks::ClientCommand::endpoint),
                   "common", &::mks::ClientCommand::common);
    };

    template <>
    struct Meta<::mks::CommonConfig, void> {
        constexpr static auto value = Object(
            "configPath",
            make_tags<mksArgparser::arg_long_name<"config">, mksArgparser::arg_short_name<'c'>,
                      mksArgparser::arg_aliases<"config">, mksArgparser::arg_value_name<"PATH">,
                      mksArgparser::arg_help<"configuration file path">>(
                &::mks::CommonConfig::configPath),
            "logLevel",
            make_tags<
                mksArgparser::arg_long_name<"log-level">, mksArgparser::arg_aliases<"log-level">,
                mksArgparser::arg_value_name<"LEVEL">,
                mksArgparser::arg_choices<"trace", "info", "warn", "error", "critical", "off">,
                mksArgparser::arg_default<"info"_cs>, mksArgparser::arg_env<"MKSYNC_LOG_LEVEL">,
                mksArgparser::arg_help<"log level">>(&::mks::CommonConfig::logLevel),
            "backend",
            make_tags<mksArgparser::arg_long_name<"backend">, mksArgparser::arg_aliases<"backend">,
                      mksArgparser::arg_value_name<"NAME">, mksArgparser::arg_env<"MKSYNC_BACKEND">,
                      mksArgparser::arg_help<"platform backend (default: auto)">>(
                &::mks::CommonConfig::backend));
    };

    template <>
    struct Meta<::mks::CheckPlatformCommand, void> {
        constexpr static auto value = Object(
            "backend",
            make_tags<mksArgparser::arg_long_name<"backend">, mksArgparser::arg_value_name<"NAME">,
                      mksArgparser::arg_env<"MKSYNC_BACKEND">,
                      mksArgparser::arg_help<"backend to check (default: auto)">>(
                &::mks::CheckPlatformCommand::backend));
    };

    template <>
    struct Meta<::mks::BackendCommand, void> {
        constexpr static auto value =
            Object("list",
                   make_tags<mksArgparser::arg_long_name<"list">,
                             mksArgparser::arg_help<"list registered backends">,
                             mksArgparser::ArgTags{.flag = true}>(&::mks::BackendCommand::list),
                   "checked",
                   make_tags<mksArgparser::arg_long_name<"checked">,
                             mksArgparser::arg_help<"run each backend check and show capabilities">,
                             mksArgparser::ArgTags{.flag = true}>(&::mks::BackendCommand::checked));
    };

    template <>
    struct Meta<::mks::CliCommands, void> {
        constexpr static auto value = Object(
            "server",
            make_tags<mksArgparser::arg_help<"run as server">,
                      mksArgparser::ArgTags{.command = true}>(&::mks::CliCommands::server),
            "client",
            make_tags<mksArgparser::arg_help<"run as client">,
                      mksArgparser::ArgTags{.command = true}>(&::mks::CliCommands::client),
            "checkPlatform",
            make_tags<mksArgparser::arg_long_name<"check-platform">,
                      mksArgparser::arg_help<"validate platform capture and injection backends">,
                      mksArgparser::ArgTags{.command = true}>(&::mks::CliCommands::checkPlatform),
            "backend",
            make_tags<mksArgparser::arg_help<"list and check platform backends">,
                      mksArgparser::ArgTags{.command = true}>(&::mks::CliCommands::backend));
    };

} // namespace NekoProto
