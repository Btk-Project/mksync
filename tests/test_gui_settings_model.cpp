#include "model/settings_model.hpp"

#include <gtest/gtest.h>

namespace
{

    TEST(GuiSettingsModel, SavesAndImportsTheCliConfigurationFormat)
    {
        const auto path = std::filesystem::temp_directory_path() / "mksync-gui-settings-model.json";
        std::filesystem::remove(path);

        auto saved = mks::gui::SettingsModel{};
        saved.createNew();
        saved.config().machineId = "gui-machine";
        mks::upsertScreenLayout(saved.config(), mks::ScreenLayoutConfig{
                                                    .ownerId     = "gui-machine",
                                                    .screenIndex = 0,
                                                    .cell        = {.x = -1, .y = 2},
        });
        saved.config().trustedClients.push_back({.machineId = "peer-machine", .name = "peer"});

        ASSERT_TRUE(saved.saveAs(path).has_value());
        EXPECT_EQ(saved.path(), path);

        auto imported = mks::gui::SettingsModel{};
        ASSERT_TRUE(imported.importFrom(path).has_value());
        EXPECT_EQ(imported.path(), path);
        EXPECT_EQ(imported.config().machineId, "gui-machine");
        ASSERT_EQ(imported.config().screens.size(), 1U);
        EXPECT_EQ(imported.config().screens[0].cell, (mks::GridPosition{.x = -1, .y = 2}));
        ASSERT_EQ(imported.config().trustedClients.size(), 1U);
        EXPECT_EQ(imported.config().trustedClients[0].name, "peer");

        std::filesystem::remove(path);
    }

} // namespace
