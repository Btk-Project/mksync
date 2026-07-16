#include "preinclude.hpp"
#include "config/arg_config.hpp"

#include <spdlog/spdlog.h>

#include <array>

MKS_BEGIN

auto makeCliParserConfig() -> NekoProto::argparser::ArgParserConfig
{
    auto config        = NekoProto::argparser::ArgParserConfig{};
    config.programName = "mksync";
    config.description = "mksync - simple mouse keyboard tools";
    config.version     = MKS_VERSION_STR;
    config.configIo.emplace();
    config.configIo->enableFormat("toml");
    config.deprecatedOptionHandler = [](std::string_view optionName, std::string_view message) {
        SPDLOG_WARN("Option '{}' is deprecated: {}", optionName, message);
    };
    return config;
}

auto parseCliArguments(int argc, const char *const *argv) -> IoResult<CliCommand>
{
    auto parsed = NekoProto::argparser::parser<CliCommands>(argc, argv, makeCliParserConfig());
    if (!parsed) {
        return Err(parsed.error());
    }
    return std::move(*parsed);
}

auto importCliCommand(const std::filesystem::path &path) -> IoResult<CliCommand>
{
    const auto pathText = path.string();
    const auto argv     = std::array<const char *, 3>{
        "mksync",
        "--import-toml",
        pathText.c_str(),
    };
    return parseCliArguments(static_cast<int>(argv.size()), argv.data());
}

auto exportCliCommand(const std::filesystem::path &path, const CliCommand &command)
    -> IoResult<void>
{
    // This is the same metadata-driven exporter invoked by parser() for --export-toml.
    auto selection = NekoProto::argparser::detail::ConfigIoSelection{};
    selection.export_files.push_back({.format = "toml", .path = path.string()});
    if (auto error = NekoProto::argparser::detail::export_command_config_files<CliCommands>(
            selection, command)) {
        return Err(error);
    }
    return {};
}

MKS_END
