#include "model/settings_model.hpp"
#include "preinclude.hpp"

namespace mks::gui
{

    auto SettingsModel::createNew() -> void
    {
        mConfig = AppConfig{};
        ensureMachineId(mConfig);
        mPath.clear();
    }

    auto SettingsModel::loadOrCreate(const std::filesystem::path &path) -> IoResult<void>
    {
        ILIAS_TRY(auto config, mks::loadOrCreateConfig(path));
        mConfig = std::move(config);
        mPath   = path;
        return {};
    }

    auto SettingsModel::importFrom(const std::filesystem::path &path) -> IoResult<void>
    {
        ILIAS_TRY(auto config, mks::loadConfig(path));
        ensureMachineId(config);
        mConfig = std::move(config);
        mPath   = path;
        return {};
    }

    auto SettingsModel::save() -> IoResult<void>
    {
        if (mPath.empty()) {
            return Err(ConfigError::InvalidConfig);
        }
        return mks::saveConfig(mPath, mConfig);
    }

    auto SettingsModel::saveAs(const std::filesystem::path &path) -> IoResult<void>
    {
        ILIAS_TRYV(mks::saveConfig(path, mConfig));
        mPath = path;
        return {};
    }

    auto SettingsModel::config() const -> const AppConfig &
    {
        return mConfig;
    }

    auto SettingsModel::config() -> AppConfig &
    {
        return mConfig;
    }

    auto SettingsModel::path() const -> const std::filesystem::path &
    {
        return mPath;
    }

} // namespace mks::gui
