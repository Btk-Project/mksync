#include "config/arg_config.hpp"
#include "model/settings_model.hpp"

#include <gtest/gtest.h>

#include <array>

namespace
{

    TEST(GuiSettingsModel, SavesAndImportsTheScreenConfigurationFormat)
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

    TEST(GuiStartupSettings, ImportsTomlExportedByTheCliParser)
    {
        const auto path = std::filesystem::temp_directory_path() / "mksync-gui-startup-cli.toml";
        std::filesystem::remove(path);
        const auto pathText = path.string();
        const auto argv     = std::array<const char *, 9>{
            "mksync",      "server", "0.0.0.0:4321",  "--config",       "screens.json",
            "--log-level", "trace",  "--export-toml", pathText.c_str(),
        };

        auto parsed = mks::parseCliArguments(static_cast<int>(argv.size()), argv.data());
        ASSERT_TRUE(parsed.has_value()) << parsed.error().message();
        ASSERT_TRUE(std::holds_alternative<mks::ServerCommand>(*parsed));

        auto imported = mks::importCliCommand(path);
        ASSERT_TRUE(imported.has_value()) << imported.error().message();
        const auto &server = std::get<mks::ServerCommand>(*imported);
        EXPECT_EQ(server.endpoint, "0.0.0.0:4321");
        EXPECT_EQ(server.common.configPath, "screens.json");
        EXPECT_EQ(server.common.logLevel, "trace");

        std::filesystem::remove(path);
    }

    TEST(GuiStartupSettings, ExportsTheSharedCliCommandModel)
    {
        const auto path = std::filesystem::temp_directory_path() / "mksync-gui-startup-model.toml";
        std::filesystem::remove(path);
        const auto command = mks::CliCommand{
            mks::ClientCommand{
                               .endpoint = "192.0.2.8:9876",
                               .common =
                    {
                        .configPath = "client-screens.json",
                        .logLevel   = "warn",
                        .backend    = "wayland-wlr",
                    }, }
        };

        auto exported = mks::exportCliCommand(path, command);
        ASSERT_TRUE(exported.has_value()) << exported.error().message();

        auto imported = mks::importCliCommand(path);
        ASSERT_TRUE(imported.has_value()) << imported.error().message();
        const auto &client = std::get<mks::ClientCommand>(*imported);
        EXPECT_EQ(client.endpoint, "192.0.2.8:9876");
        EXPECT_EQ(client.common.configPath, "client-screens.json");
        EXPECT_EQ(client.common.logLevel, "warn");
        EXPECT_EQ(client.common.backend, "wayland-wlr");

        std::filesystem::remove(path);
    }

    TEST(CliBackendOptions, ParsesBackendSelectionForServer)
    {
        const auto argv   = std::array<const char *, 5>{"mksync", "server", "0.0.0.0:4321",
                                                        "--backend", "wayland-wlr"};
        auto       parsed = mks::parseCliArguments(static_cast<int>(argv.size()), argv.data());
        ASSERT_TRUE(parsed.has_value()) << parsed.error().message();
        ASSERT_TRUE(std::holds_alternative<mks::ServerCommand>(*parsed));
        EXPECT_EQ(std::get<mks::ServerCommand>(*parsed).common.backend, "wayland-wlr");
    }

    TEST(CliBackendOptions, ParsesCheckedBackendListing)
    {
        const auto argv   = std::array<const char *, 4>{"mksync", "backend", "--list", "--checked"};
        auto       parsed = mks::parseCliArguments(static_cast<int>(argv.size()), argv.data());
        ASSERT_TRUE(parsed.has_value()) << parsed.error().message();
        ASSERT_TRUE(std::holds_alternative<mks::BackendCommand>(*parsed));
        const auto &backend = std::get<mks::BackendCommand>(*parsed);
        EXPECT_TRUE(backend.list);
        EXPECT_TRUE(backend.checked);
    }

} // namespace
