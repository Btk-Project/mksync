#include "config/app_config.hpp"

#include <gtest/gtest.h>

#include <filesystem>

namespace {

auto makeConfig() -> mks::AppConfig {
    return mks::AppConfig {
        .version = 1,
        .machineId = "machine-local",
        .screens = {
            mks::ScreenLayoutConfig {
                .ownerId = "machine-local",
                .screenIndex = 0,
                .cell = {.x = 0, .y = 0},
            },
            mks::ScreenLayoutConfig {
                .ownerId = "machine-remote",
                .screenIndex = 1,
                .cell = {.x = 2, .y = -1},
            },
        },
        .trustedClients = {
            mks::TrustedClientConfig {
                .machineId = "machine-remote",
                .name = "remote",
            },
        },
    };
}

} // namespace

TEST(AppConfig, SerializesAndDeserializes) {
    auto text = mks::serializeConfig(makeConfig());
    ASSERT_TRUE(text.has_value()) << text.error().message();
    EXPECT_NE(text->find("machine-local"), std::string::npos);

    auto config = mks::deserializeConfig(*text);
    ASSERT_TRUE(config.has_value()) << config.error().message();
    EXPECT_EQ(config->machineId, "machine-local");
    ASSERT_EQ(config->screens.size(), 2U);
    EXPECT_EQ(config->screens[1].cell, (mks::GridPosition {.x = 2, .y = -1}));
    ASSERT_EQ(config->trustedClients.size(), 1U);
    EXPECT_EQ(config->trustedClients[0].machineId, "machine-remote");
}

TEST(AppConfig, FindsScreenLayout) {
    auto config = makeConfig();

    auto local = mks::findScreenLayout(config, "machine-local", 0);
    ASSERT_TRUE(local.has_value());
    EXPECT_EQ(*local, (mks::GridPosition {.x = 0, .y = 0}));

    auto missing = mks::findScreenLayout(config, "machine-local", 3);
    EXPECT_FALSE(missing.has_value());
}

TEST(AppConfig, UpsertsScreenLayout) {
    auto config = makeConfig();

    mks::upsertScreenLayout(config, mks::ScreenLayoutConfig {
        .ownerId = "machine-local",
        .screenIndex = 0,
        .cell = {.x = 5, .y = 6},
    });
    ASSERT_EQ(config.screens.size(), 2U);
    auto local = mks::findScreenLayout(config, "machine-local", 0);
    ASSERT_TRUE(local.has_value());
    EXPECT_EQ(*local, (mks::GridPosition {.x = 5, .y = 6}));

    mks::upsertScreenLayout(config, mks::ScreenLayoutConfig {
        .ownerId = "machine-new",
        .screenIndex = 0,
        .cell = {.x = -1, .y = 2},
    });
    ASSERT_EQ(config.screens.size(), 3U);
    auto added = mks::findScreenLayout(config, "machine-new", 0);
    ASSERT_TRUE(added.has_value());
    EXPECT_EQ(*added, (mks::GridPosition {.x = -1, .y = 2}));
}

TEST(AppConfig, ChecksTrustedClients) {
    auto openConfig = mks::AppConfig {};
    EXPECT_TRUE(mks::isTrustedClient(openConfig, {}, "any-client"));

    auto config = makeConfig();
    EXPECT_TRUE(mks::isTrustedClient(config, "machine-remote", {}));
    EXPECT_TRUE(mks::isTrustedClient(config, {}, "remote"));
    EXPECT_FALSE(mks::isTrustedClient(config, "machine-other", "other"));
}

TEST(AppConfig, GeneratesMachineIdWhenMissing) {
    auto config = mks::AppConfig {};

    auto first = mks::ensureMachineId(config);
    EXPECT_FALSE(first.empty());
    EXPECT_EQ(config.machineId.substr(0, 4), "mks-");

    auto second = mks::ensureMachineId(config);
    EXPECT_EQ(first, second);
}

TEST(AppConfig, SavesAndLoadsFile) {
    auto path = std::filesystem::temp_directory_path() / "mksync-test-config.json";
    std::filesystem::remove(path);

    auto saved = mks::saveConfig(path, makeConfig());
    ASSERT_TRUE(saved.has_value()) << saved.error().message();

    auto loaded = mks::loadConfig(path);
    ASSERT_TRUE(loaded.has_value()) << loaded.error().message();
    EXPECT_EQ(loaded->machineId, "machine-local");

    std::filesystem::remove(path);
}

TEST(AppConfig, LoadOrCreateCreatesMissingFile) {
    auto path = std::filesystem::temp_directory_path() / "mksync-test-created-config.json";
    std::filesystem::remove(path);

    auto created = mks::loadOrCreateConfig(path);
    ASSERT_TRUE(created.has_value()) << created.error().message();
    EXPECT_FALSE(created->machineId.empty());
    EXPECT_TRUE(std::filesystem::exists(path));

    auto loaded = mks::loadOrCreateConfig(path);
    ASSERT_TRUE(loaded.has_value()) << loaded.error().message();
    EXPECT_EQ(loaded->machineId, created->machineId);

    std::filesystem::remove(path);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
