#pragma once

#include "config/config.hpp"

#include <nekoproto/argparser/argparser.hpp>

#include <string>

MKS_BEGIN

struct ServerCommand {
    std::string endpoint;
    std::string configPath = "mksync.json";
};

struct ClientCommand {
    std::string endpoint;
    std::string configPath = "mksync.json";
};

using CheckPlatformCommand = NekoProto::argparser::ArgCommand<1>;

struct CliCommands {
    ServerCommand        server;
    ClientCommand        client;
    CheckPlatformCommand checkPlatform;
};

MKS_END

namespace NekoProto
{

    namespace mksArgparser = argparser;

    template <>
    struct Meta<::mks::ServerCommand, void> {
        constexpr static auto value = Object(
            "endpoint",
            make_tags<mksArgparser::arg_value_name<"HOST:PORT">,
                      mksArgparser::arg_help<"listen endpoint">,
                      mksArgparser::ArgTags{.required = true, .positional = true}>(
                &::mks::ServerCommand::endpoint),
            "configPath",
            make_tags<mksArgparser::arg_long_name<"config">, mksArgparser::arg_short_name<'c'>,
                      mksArgparser::arg_value_name<"PATH">,
                      mksArgparser::arg_help<"configuration file path">>(
                &::mks::ServerCommand::configPath));
    };

    template <>
    struct Meta<::mks::ClientCommand, void> {
        constexpr static auto value = Object(
            "endpoint",
            make_tags<mksArgparser::arg_value_name<"HOST:PORT">,
                      mksArgparser::arg_help<"server endpoint">,
                      mksArgparser::ArgTags{.required = true, .positional = true}>(
                &::mks::ClientCommand::endpoint),
            "configPath",
            make_tags<mksArgparser::arg_long_name<"config">, mksArgparser::arg_short_name<'c'>,
                      mksArgparser::arg_value_name<"PATH">,
                      mksArgparser::arg_help<"configuration file path">>(
                &::mks::ClientCommand::configPath));
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
                      mksArgparser::ArgTags{.command = true}>(&::mks::CliCommands::checkPlatform));
    };

} // namespace NekoProto
