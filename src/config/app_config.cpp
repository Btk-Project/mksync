#include "app_config.hpp"

#include "refl/serde.hpp"

#include <array>
#include <cerrno>
#include <fstream>
#include <random>

MKS_BEGIN

THIS_ERROR_IMPL(ConfigError);

namespace {

auto streamError() -> std::error_code {
    if (errno != 0) {
        return {errno, std::generic_category()};
    }
    return ConfigError::IoError;
}

auto randomBytes() -> std::array<std::byte, 16> {
    auto rd = std::random_device {};
    auto bytes = std::array<std::byte, 16> {};
    for (auto &byte : bytes) {
        byte = static_cast<std::byte>(rd() & 0xFFU);
    }
    return bytes;
}

} // namespace

auto generateMachineId() -> std::string {
    static constexpr auto digits = std::string_view {"0123456789abcdef"};
    auto bytes = randomBytes();
    auto result = std::string {"mks-"};
    result.reserve(4 + bytes.size() * 2);

    for (auto byte : bytes) {
        const auto value = static_cast<unsigned>(byte);
        result.push_back(digits[(value >> 4U) & 0xFU]);
        result.push_back(digits[value & 0xFU]);
    }
    return result;
}

auto ensureMachineId(AppConfig &config) -> std::string_view {
    if (config.machineId.empty()) {
        config.machineId = generateMachineId();
    }
    return config.machineId;
}

auto findScreenLayout(
    const AppConfig &config,
    std::string_view ownerId,
    uint32_t screenIndex
) -> std::optional<GridPosition> {
    auto it = std::ranges::find_if(config.screens, [&](const auto &layout) {
        return layout.ownerId == ownerId && layout.screenIndex == screenIndex;
    });
    if (it == config.screens.end()) {
        return std::nullopt;
    }
    return it->cell;
}

auto upsertScreenLayout(AppConfig &config, ScreenLayoutConfig layout) -> void {
    auto it = std::ranges::find_if(config.screens, [&](const auto &current) {
        return current.ownerId == layout.ownerId && current.screenIndex == layout.screenIndex;
    });
    if (it == config.screens.end()) {
        config.screens.push_back(std::move(layout));
        return;
    }

    *it = std::move(layout);
}

auto isTrustedClient(
    const AppConfig &config,
    std::string_view machineId,
    std::string_view name
) -> bool {
    if (config.trustedClients.empty()) {
        return true;
    }

    return std::ranges::any_of(config.trustedClients, [&](const auto &client) {
        const auto machineMatches = !machineId.empty() && client.machineId == machineId;
        const auto nameMatches = !name.empty() && client.name == name;
        return machineMatches || nameMatches;
    });
}

auto serializeConfig(const AppConfig &config) -> IoResult<std::string> {
    auto buffer = std::vector<char> {};
    {
        auto serializer = Serializer {buffer};
        if (!serializer(config)) {
            return Err(ConfigError::SerializeFailed);
        }
    }
    return std::string {buffer.begin(), buffer.end()};
}

auto deserializeConfig(std::string_view text) -> IoResult<AppConfig> {
    auto config = AppConfig {};
    auto deserializer = Deserializer {text.data(), text.size()};
    if (!deserializer(config)) {
        return Err(ConfigError::DeserializeFailed);
    }
    if (config.version == 0) {
        return Err(ConfigError::InvalidConfig);
    }
    return config;
}

auto loadConfig(const std::filesystem::path &path) -> IoResult<AppConfig> {
    errno = 0;
    auto input = std::ifstream {path, std::ios::binary};
    if (!input) {
        return Err(streamError());
    }

    auto text = std::string {
        std::istreambuf_iterator<char> {input},
        std::istreambuf_iterator<char> {}
    };
    if (input.bad()) {
        return Err(streamError());
    }
    return deserializeConfig(text);
}

auto loadOrCreateConfig(const std::filesystem::path &path) -> IoResult<AppConfig> {
    auto error = std::error_code {};
    const auto exists = std::filesystem::exists(path, error);
    if (error) {
        return Err(error);
    }

    if (!exists) {
        auto config = AppConfig {};
        ensureMachineId(config);
        ILIAS_TRYV(saveConfig(path, config));
        return config;
    }

    ILIAS_TRY(auto config, loadConfig(path));
    const auto oldMachineId = config.machineId;
    ensureMachineId(config);
    if (config.machineId != oldMachineId) {
        ILIAS_TRYV(saveConfig(path, config));
    }
    return config;
}

auto saveConfig(const std::filesystem::path &path, const AppConfig &config) -> IoResult<void> {
    if (path.has_parent_path()) {
        auto error = std::error_code {};
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) {
            return Err(error);
        }
    }

    ILIAS_TRY(auto text, serializeConfig(config));

    errno = 0;
    auto output = std::ofstream {path, std::ios::binary | std::ios::trunc};
    if (!output) {
        return Err(streamError());
    }

    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!output) {
        return Err(streamError());
    }
    return {};
}

MKS_END
