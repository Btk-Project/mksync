#pragma once

#include "config/app_config.hpp"
#include "preinclude.hpp"

#include <filesystem>

namespace mks::gui
{

    // The Model owns the exact AppConfig format shared with the command-line app.
    // It deliberately has no Qt dependency so configuration behavior remains easy
    // to reuse and test independently from the visual layer.
    class SettingsModel final {
    public:
        auto createNew() -> void;
        auto loadOrCreate(const std::filesystem::path &path) -> IoResult<void>;
        auto importFrom(const std::filesystem::path &path) -> IoResult<void>;
        auto save() -> IoResult<void>;
        auto saveAs(const std::filesystem::path &path) -> IoResult<void>;

        auto config() const -> const AppConfig &;
        auto config() -> AppConfig &;
        auto path() const -> const std::filesystem::path &;

    private:
        AppConfig             mConfig;
        std::filesystem::path mPath;
    };

} // namespace mks::gui
