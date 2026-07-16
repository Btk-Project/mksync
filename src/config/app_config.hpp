#pragma once

#include "preinclude.hpp"
#include "core.hpp"
#include "refl/formatter.hpp"

#include <filesystem>
#include <optional>
#include <string_view>

MKS_BEGIN

enum class ConfigError {
    Ok = 0,
    InvalidConfig,
    SerializeFailed,
    DeserializeFailed,
    IoError,
};
THIS_ERROR(ConfigError);

struct ScreenLayoutConfig {
    std::string ownerId;
    uint32_t screenIndex = 0;
    GridPosition cell;
};
FORMATTER(ScreenLayoutConfig);

struct TrustedClientConfig {
    std::string machineId;
    std::string name;
};
FORMATTER(TrustedClientConfig);

struct AppConfig {
    uint32_t version = 1;
    std::string machineId;
    std::vector<ScreenLayoutConfig> screens;
    std::vector<TrustedClientConfig> trustedClients;
};
FORMATTER(AppConfig);

auto generateMachineId() -> std::string;
auto ensureMachineId(AppConfig &config) -> std::string_view;
auto findScreenLayout(
    const AppConfig &config,
    std::string_view ownerId,
    uint32_t screenIndex
) -> std::optional<GridPosition>;
auto upsertScreenLayout(AppConfig &config, ScreenLayoutConfig layout) -> void;
auto isTrustedClient(
    const AppConfig &config,
    std::string_view machineId,
    std::string_view name
) -> bool;

auto serializeConfig(const AppConfig &config) -> IoResult<std::string>;
auto deserializeConfig(std::string_view text) -> IoResult<AppConfig>;
auto loadConfig(const std::filesystem::path &path) -> IoResult<AppConfig>;
auto loadOrCreateConfig(const std::filesystem::path &path) -> IoResult<AppConfig>;
auto saveConfig(const std::filesystem::path &path, const AppConfig &config) -> IoResult<void>;

MKS_END

REFL_REGISTER_FMT_FORMATTER(mks::ConfigError);
REFL_REGISTER_FMT_FORMATTER(mks::ScreenLayoutConfig);
REFL_REGISTER_FMT_FORMATTER(mks::TrustedClientConfig);
REFL_REGISTER_FMT_FORMATTER(mks::AppConfig);
