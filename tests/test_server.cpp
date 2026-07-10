#include "app/server.hpp"
#include "platform/platform.hpp"
#include "support/mock_platform.hpp"

#include <gtest/gtest.h>
#include <ilias/testing.hpp>
#include <filesystem>
#include <stdexcept>

auto mks::Platform::create() -> Ptr {
    return nullptr;
}

namespace {

auto makeEndpoint(uint16_t port) -> mks::IPEndpoint {
    auto endpoint = mks::IPEndpoint::fromString(fmtlib::format("127.0.0.1:{}", port));
    if (!endpoint) {
        throw std::runtime_error("invalid test endpoint");
    }
    return *endpoint;
}

auto makeScreen(std::string name, int32_t width, int32_t height, bool primary) -> mks::ScreenInfo {
    return mks::ScreenInfo {
        .x = 0,
        .y = 0,
        .width = width,
        .height = height,
        .dpi = 72,
        .name = std::move(name),
        .primary = primary,
    };
}

auto findScreen(
    const std::vector<mks::TopologyScreen> &screens,
    std::string_view ownerId,
    uint32_t screenIndex
) -> const mks::TopologyScreen * {
    auto it = std::ranges::find_if(screens, [&](const auto &screen) {
        return screen.key.ownerId == ownerId && screen.key.screenIndex == screenIndex;
    });
    if (it == screens.end()) {
        return nullptr;
    }
    return &*it;
}

} // namespace

TEST(ServerScreenRegistry, RegistersLocalPrimaryAtOrigin) {
    auto endpoint = makeEndpoint(30001);
    auto server = mks::Server {endpoint};

    server.registerScreensForTest(endpoint, {
        makeScreen("local-primary", 1920, 1080, true),
        makeScreen("local-side", 1280, 720, false),
    }, true);

    auto screens = server.topologyScreens();
    ASSERT_EQ(screens.size(), 2U);

    auto ownerId = fmtlib::format("{}", endpoint);
    auto *primary = findScreen(screens, ownerId, 0);
    ASSERT_NE(primary, nullptr);
    EXPECT_TRUE(primary->local);
    EXPECT_EQ(primary->cell, (mks::GridPosition {.x = 0, .y = 0}));
    EXPECT_EQ(primary->info.name, "local-primary");

    auto *side = findScreen(screens, ownerId, 1);
    ASSERT_NE(side, nullptr);
    EXPECT_TRUE(side->local);
    EXPECT_EQ(side->cell, (mks::GridPosition {.x = 1, .y = 0}));
}

TEST(ServerScreenRegistry, RegistersRemoteScreensInFreeRightCells) {
    auto localEndpoint = makeEndpoint(30002);
    auto remoteEndpoint = makeEndpoint(30003);
    auto server = mks::Server {localEndpoint};

    server.registerScreensForTest(localEndpoint, {
        makeScreen("local-primary", 1920, 1080, true),
    }, true);
    server.registerScreensForTest(remoteEndpoint, {
        makeScreen("remote-primary", 2560, 1440, true),
        makeScreen("remote-side", 1600, 900, false),
    }, false);

    auto screens = server.topologyScreens();
    ASSERT_EQ(screens.size(), 3U);

    auto remoteOwnerId = fmtlib::format("{}", remoteEndpoint);
    auto *remotePrimary = findScreen(screens, remoteOwnerId, 0);
    ASSERT_NE(remotePrimary, nullptr);
    EXPECT_FALSE(remotePrimary->local);
    EXPECT_EQ(remotePrimary->cell, (mks::GridPosition {.x = 1, .y = 0}));
    EXPECT_EQ(remotePrimary->info.width, 2560);

    auto *remoteSide = findScreen(screens, remoteOwnerId, 1);
    ASSERT_NE(remoteSide, nullptr);
    EXPECT_FALSE(remoteSide->local);
    EXPECT_EQ(remoteSide->cell, (mks::GridPosition {.x = 2, .y = 0}));
}

TEST(ServerScreenRegistry, ReplacesScreensForSameEndpoint) {
    auto endpoint = makeEndpoint(30004);
    auto server = mks::Server {endpoint};

    server.registerScreensForTest(endpoint, {
        makeScreen("old", 1920, 1080, true),
    }, true);
    server.registerScreensForTest(endpoint, {
        makeScreen("new-primary", 3840, 2160, true),
        makeScreen("new-side", 1920, 1080, false),
    }, true);

    auto screens = server.topologyScreens();
    ASSERT_EQ(screens.size(), 2U);

    auto ownerId = fmtlib::format("{}", endpoint);
    auto *primary = findScreen(screens, ownerId, 0);
    ASSERT_NE(primary, nullptr);
    EXPECT_EQ(primary->info.name, "new-primary");
    EXPECT_EQ(primary->info.width, 3840);
    EXPECT_EQ(primary->cell, (mks::GridPosition {.x = 0, .y = 0}));
}

TEST(ServerScreenRegistry, UsesConfiguredScreenCells) {
    auto localEndpoint = makeEndpoint(30014);
    auto remoteEndpoint = makeEndpoint(30015);
    auto localOwnerId = std::string {"machine-local"};
    auto remoteOwnerId = std::string {"machine-remote"};
    auto config = mks::AppConfig {
        .machineId = localOwnerId,
        .screens = {
            mks::ScreenLayoutConfig {
                .ownerId = localOwnerId,
                .screenIndex = 0,
                .cell = {.x = -1, .y = 0},
            },
            mks::ScreenLayoutConfig {
                .ownerId = remoteOwnerId,
                .screenIndex = 0,
                .cell = {.x = 0, .y = 1},
            },
        },
        .trustedClients = {},
    };
    auto server = mks::Server {localEndpoint, std::move(config)};

    server.registerScreensForTest(localEndpoint, {
        makeScreen("local-primary", 1920, 1080, true),
    }, true);
    server.registerScreensForTest(remoteEndpoint, remoteOwnerId, {
        makeScreen("remote-primary", 2560, 1440, true),
    }, false);

    auto screens = server.topologyScreens();
    ASSERT_EQ(screens.size(), 2U);

    auto *local = findScreen(screens, localOwnerId, 0);
    ASSERT_NE(local, nullptr);
    EXPECT_EQ(local->cell, (mks::GridPosition {.x = -1, .y = 0}));

    auto *remote = findScreen(screens, remoteOwnerId, 0);
    ASSERT_NE(remote, nullptr);
    EXPECT_EQ(remote->cell, (mks::GridPosition {.x = 0, .y = 1}));
}

TEST(ServerScreenRegistry, RepairsStaleRemoteCellAfterLocalMonitorAdded) {
    auto localEndpoint = makeEndpoint(30026);
    auto remoteEndpoint = makeEndpoint(30027);
    auto localOwnerId = std::string {"machine-local"};
    auto remoteOwnerId = std::string {"machine-remote"};
    auto server = mks::Server {localEndpoint, mks::AppConfig {
        .machineId = localOwnerId,
        // This is an auto-saved position from when the server only had its
        // primary monitor. A newly attached local monitor now takes (1, 0).
        .screens = {
            mks::ScreenLayoutConfig {
                .ownerId = localOwnerId,
                .screenIndex = 0,
                .cell = {.x = 0, .y = 0},
            },
            mks::ScreenLayoutConfig {
                .ownerId = remoteOwnerId,
                .screenIndex = 0,
                .cell = {.x = 1, .y = 0},
            },
        },
        .trustedClients = {},
    }};

    server.registerScreensForTest(localEndpoint, localOwnerId, {
        makeScreen("local-primary", 1920, 1080, true),
        makeScreen("local-side", 1280, 720, false),
    }, true);
    server.registerScreensForTest(remoteEndpoint, remoteOwnerId, {
        makeScreen("remote-primary", 2560, 1440, true),
    }, false);

    auto screens = server.topologyScreens();
    ASSERT_EQ(screens.size(), 3U);

    auto *localSide = findScreen(screens, localOwnerId, 1);
    ASSERT_NE(localSide, nullptr);
    EXPECT_EQ(localSide->cell, (mks::GridPosition {.x = 1, .y = 0}));

    auto *remote = findScreen(screens, remoteOwnerId, 0);
    ASSERT_NE(remote, nullptr);
    EXPECT_EQ(remote->cell, (mks::GridPosition {.x = 2, .y = 0}));

    auto persistedRemote = mks::findScreenLayout(server.configForTest(), remoteOwnerId, 0);
    ASSERT_TRUE(persistedRemote.has_value());
    EXPECT_EQ(*persistedRemote, (mks::GridPosition {.x = 2, .y = 0}));

    server.handleInputEventForTest(mks::InputEvent {mks::MouseMoveEvent {
        .x = 1279,
        .y = 360,
        .screenIndex = 1,
    }});

    EXPECT_EQ(server.activeScreenKey(), remote->key);
}

TEST(ServerScreenRegistry, PersistsRegisteredScreenCellsToConfig) {
    auto localEndpoint = makeEndpoint(30018);
    auto remoteEndpoint = makeEndpoint(30019);
    auto path = std::filesystem::temp_directory_path() / "mksync-test-server-layout.json";
    std::filesystem::remove(path);

    auto server = mks::Server {localEndpoint, mks::AppConfig {
        .version = 1,
        .machineId = "machine-local",
        .screens = {},
        .trustedClients = {},
    }, path};

    server.registerScreensForTest(localEndpoint, {
        makeScreen("local-primary", 1920, 1080, true),
    }, true);
    server.registerScreensForTest(remoteEndpoint, "machine-remote", {
        makeScreen("remote-primary", 2560, 1440, true),
    }, false);

    auto local = mks::findScreenLayout(server.configForTest(), "machine-local", 0);
    ASSERT_TRUE(local.has_value());
    EXPECT_EQ(*local, (mks::GridPosition {.x = 0, .y = 0}));

    auto remote = mks::findScreenLayout(server.configForTest(), "machine-remote", 0);
    ASSERT_TRUE(remote.has_value());
    EXPECT_EQ(*remote, (mks::GridPosition {.x = 1, .y = 0}));

    auto loaded = mks::loadConfig(path);
    ASSERT_TRUE(loaded.has_value()) << loaded.error().message();
    auto loadedRemote = mks::findScreenLayout(*loaded, "machine-remote", 0);
    ASSERT_TRUE(loadedRemote.has_value());
    EXPECT_EQ(*loadedRemote, (mks::GridPosition {.x = 1, .y = 0}));

    auto restartedLocalEndpoint = makeEndpoint(30020);
    auto restartedRemoteEndpoint = makeEndpoint(30021);
    auto restarted = mks::Server {restartedLocalEndpoint, *loaded};
    restarted.registerScreensForTest(restartedRemoteEndpoint, "machine-remote", {
        makeScreen("remote-primary", 2560, 1440, true),
    }, false);
    restarted.registerScreensForTest(restartedLocalEndpoint, {
        makeScreen("local-primary", 1920, 1080, true),
    }, true);

    auto restartedScreens = restarted.topologyScreens();
    auto *restartedLocal = findScreen(restartedScreens, "machine-local", 0);
    ASSERT_NE(restartedLocal, nullptr);
    EXPECT_EQ(restartedLocal->cell, (mks::GridPosition {.x = 0, .y = 0}));

    auto *restartedRemote = findScreen(restartedScreens, "machine-remote", 0);
    ASSERT_NE(restartedRemote, nullptr);
    EXPECT_EQ(restartedRemote->cell, (mks::GridPosition {.x = 1, .y = 0}));

    std::filesystem::remove(path);
}

TEST(ServerSecurity, AllowsAllClientsWhenTrustedListIsEmpty) {
    auto endpoint = makeEndpoint(30016);
    auto server = mks::Server {endpoint};

    EXPECT_TRUE(server.isClientTrustedForTest("any-client"));
}

TEST(ServerSecurity, RejectsClientsMissingFromTrustedList) {
    auto endpoint = makeEndpoint(30017);
    auto server = mks::Server {endpoint, mks::AppConfig {
        .machineId = "server",
        .screens = {},
        .trustedClients = {
            mks::TrustedClientConfig {
                .machineId = "client-a",
                .name = "allowed-client",
            },
        },
    }};

    EXPECT_TRUE(server.isClientTrustedForTest("allowed-client"));
    EXPECT_TRUE(server.isClientTrustedForTest("client-a", "unknown-client-name"));
    EXPECT_FALSE(server.isClientTrustedForTest("unknown-client"));
    EXPECT_FALSE(server.isClientTrustedForTest("client-b", "unknown-client"));
}

TEST(ServerInputRouting, SwitchesActiveScreenAcrossRightEdge) {
    auto localEndpoint = makeEndpoint(30005);
    auto remoteEndpoint = makeEndpoint(30006);
    auto server = mks::Server {localEndpoint};

    server.registerScreensForTest(localEndpoint, {
        makeScreen("local-primary", 1920, 1080, true),
    }, true);
    server.registerScreensForTest(remoteEndpoint, {
        makeScreen("remote-primary", 2560, 1440, true),
    }, false);

    auto remoteKey = mks::ScreenKey {
        .ownerId = fmtlib::format("{}", remoteEndpoint),
        .screenIndex = 0,
    };
    ASSERT_NE(server.activeScreenKey(), remoteKey);

    server.handleInputEventForTest(mks::InputEvent {mks::MouseMoveEvent {
        .x = 1919,
        .y = 540,
        .screenIndex = 0,
    }});

    ASSERT_TRUE(server.activeScreenKey().has_value());
    EXPECT_EQ(*server.activeScreenKey(), remoteKey);
}

TEST(ServerInputRouting, RestoresLocalActiveScreenAfterRemoteDisconnect) {
    auto localEndpoint = makeEndpoint(30022);
    auto remoteEndpoint = makeEndpoint(30023);
    auto server = mks::Server {localEndpoint};

    server.registerScreensForTest(localEndpoint, {
        makeScreen("local-primary", 1920, 1080, true),
    }, true);
    server.registerScreensForTest(remoteEndpoint, {
        makeScreen("remote-primary", 2560, 1440, true),
    }, false);

    const auto localKey = mks::ScreenKey {
        .ownerId = fmtlib::format("{}", localEndpoint),
        .screenIndex = 0,
    };
    const auto remoteKey = mks::ScreenKey {
        .ownerId = fmtlib::format("{}", remoteEndpoint),
        .screenIndex = 0,
    };

    server.handleInputEventForTest(mks::InputEvent {mks::MouseMoveEvent {
        .x = 1919,
        .y = 540,
        .screenIndex = 0,
    }});

    ASSERT_TRUE(server.activeScreenKey().has_value());
    ASSERT_EQ(*server.activeScreenKey(), remoteKey);

    server.removeScreensForTest(remoteEndpoint);

    ASSERT_TRUE(server.activeScreenKey().has_value());
    EXPECT_EQ(*server.activeScreenKey(), localKey);

    server.registerScreensForTest(remoteEndpoint, {
        makeScreen("remote-primary", 2560, 1440, true),
    }, false);
    server.handleInputEventForTest(mks::InputEvent {mks::MouseMoveEvent {
        .x = 1919,
        .y = 540,
        .screenIndex = 0,
    }});

    ASSERT_TRUE(server.activeScreenKey().has_value());
    EXPECT_EQ(*server.activeScreenKey(), remoteKey);
}

TEST(ServerInputRouting, TogglesRemoteControlCaptureWhenActiveScreenChanges) {
    auto localEndpoint = makeEndpoint(30024);
    auto remoteEndpoint = makeEndpoint(30025);
    auto server = mks::Server {localEndpoint};
    auto capture = mks::test::MockInputCapture {};
    server.attachCaptureForTest(&capture);
    const auto localKey = mks::ScreenKey {
        .ownerId = fmtlib::format("{}", localEndpoint),
        .screenIndex = 0,
    };

    server.registerScreensForTest(localEndpoint, {
        makeScreen("local-primary", 1920, 1080, true),
    }, true);
    server.registerScreensForTest(remoteEndpoint, {
        makeScreen("remote-primary", 2560, 1440, true),
    }, false);

    EXPECT_FALSE(capture.remoteControlActive());

    server.handleInputEventForTest(mks::InputEvent {mks::MouseMoveEvent {
        .x = 1919,
        .y = 540,
        .screenIndex = 0,
    }});

    EXPECT_TRUE(capture.remoteControlActive());

    server.handleInputEventForTest(mks::InputEvent {mks::MouseMoveEvent {
        .x = 1900,
        .y = 540,
        .screenIndex = 0,
    }});

    EXPECT_FALSE(capture.remoteControlActive());

    server.handleInputEventForTest(mks::InputEvent {mks::MouseMoveEvent {
        .x = 1919,
        .y = 540,
        .screenIndex = 0,
    }});
    EXPECT_FALSE(capture.remoteControlActive());

    server.handleInputEventForTest(mks::InputEvent {mks::MouseMoveEvent {
        .x = 1919,
        .y = 540,
        .screenIndex = 0,
    }});
    EXPECT_TRUE(capture.remoteControlActive());

    server.handleInputEventForTest(mks::InputEvent {mks::KeyEvent {
        .key = mks::Key::F12,
        .nativeCode = 96,
        .release = false,
    }});
    EXPECT_FALSE(capture.remoteControlActive());
    EXPECT_EQ(server.activeScreenKey(), localKey);

    server.removeScreensForTest(remoteEndpoint);
    EXPECT_FALSE(capture.remoteControlActive());
}

TEST(ServerInputRouting, KeepsActiveScreenWhenEdgeHasNoNeighbor) {
    auto localEndpoint = makeEndpoint(30007);
    auto server = mks::Server {localEndpoint};

    server.registerScreensForTest(localEndpoint, {
        makeScreen("local-primary", 1920, 1080, true),
    }, true);

    auto activeBefore = server.activeScreenKey();
    ASSERT_TRUE(activeBefore.has_value());

    server.handleInputEventForTest(mks::InputEvent {mks::MouseMoveEvent {
        .x = 1919,
        .y = 540,
        .screenIndex = 0,
    }});

    ASSERT_TRUE(server.activeScreenKey().has_value());
    EXPECT_EQ(server.activeScreenKey(), activeBefore);
}

ILIAS_TEST(ServerInputRouting, SendsInputMessagesToRemoteClient) {
    auto localEndpoint = makeEndpoint(30008);
    auto remoteEndpoint = makeEndpoint(30009);
    auto server = mks::Server {localEndpoint};
    auto [sender, receiver] = ilias::mpsc::channel<mks::RpcMessage>(10);

    server.registerScreensForTest(localEndpoint, {
        makeScreen("local-primary", 1920, 1080, true),
    }, true);
    server.registerScreensForTest(remoteEndpoint, {
        makeScreen("remote-primary", 2560, 1440, true),
    }, false);
    server.attachClientSenderForTest(remoteEndpoint, sender);

    server.handleInputEventForTest(mks::InputEvent {mks::MouseMoveEvent {
        .x = 1919,
        .y = 540,
        .screenIndex = 0,
    }});

    auto entryMessage = co_await receiver.recv();
    EXPECT_TRUE(static_cast<bool>(entryMessage));
    if (!entryMessage) {
        co_return;
    }

    auto *entryInput = std::get_if<mks::InputMessage>(&*entryMessage);
    EXPECT_NE(entryInput, nullptr);
    if (!entryInput) {
        co_return;
    }

    auto *entryMove = std::get_if<mks::MouseMoveEvent>(&entryInput->event);
    EXPECT_NE(entryMove, nullptr);
    if (!entryMove) {
        co_return;
    }
    EXPECT_EQ(entryMove->x, 0);
    EXPECT_EQ(entryMove->y, 720);
    EXPECT_EQ(entryMove->screenIndex, 0U);

    server.handleInputEventForTest(mks::InputEvent {mks::MouseMoveEvent {
        .x = 1929,
        .y = 550,
        .screenIndex = 0,
    }});

    auto moveMessage = co_await receiver.recv();
    EXPECT_TRUE(static_cast<bool>(moveMessage));
    if (!moveMessage) {
        co_return;
    }

    auto *moveInput = std::get_if<mks::InputMessage>(&*moveMessage);
    EXPECT_NE(moveInput, nullptr);
    if (!moveInput) {
        co_return;
    }

    auto *remoteMove = std::get_if<mks::MouseMoveEvent>(&moveInput->event);
    EXPECT_NE(remoteMove, nullptr);
    if (!remoteMove) {
        co_return;
    }
    EXPECT_EQ(remoteMove->x, 10);
    EXPECT_EQ(remoteMove->y, 730);
    EXPECT_EQ(remoteMove->screenIndex, 0U);

    server.handleInputEventForTest(mks::InputEvent {mks::MouseButtonEvent {
        .x = 1919,
        .y = 550,
        .screenIndex = 0,
        .button = mks::MouseButton::Right,
        .release = false,
    }});

    auto buttonMessage = co_await receiver.recv();
    EXPECT_TRUE(static_cast<bool>(buttonMessage));
    if (!buttonMessage) {
        co_return;
    }

    auto *buttonInput = std::get_if<mks::InputMessage>(&*buttonMessage);
    EXPECT_NE(buttonInput, nullptr);
    if (!buttonInput) {
        co_return;
    }

    auto *button = std::get_if<mks::MouseButtonEvent>(&buttonInput->event);
    EXPECT_NE(button, nullptr);
    if (!button) {
        co_return;
    }
    EXPECT_EQ(button->x, 10);
    EXPECT_EQ(button->y, 730);
    EXPECT_EQ(button->screenIndex, 0U);
    EXPECT_EQ(button->button, mks::MouseButton::Right);
    EXPECT_FALSE(button->release);

    server.handleInputEventForTest(mks::InputEvent {mks::KeyEvent {
        .key = mks::Key::A,
        .modifiers = mks::KeyModifier::LeftCtrl,
        .nativeCode = 30,
        .repeat = false,
        .release = false,
    }});

    auto keyMessage = co_await receiver.recv();
    EXPECT_TRUE(static_cast<bool>(keyMessage));
    if (!keyMessage) {
        co_return;
    }

    auto *keyInput = std::get_if<mks::InputMessage>(&*keyMessage);
    EXPECT_NE(keyInput, nullptr);
    if (!keyInput) {
        co_return;
    }

    auto *key = std::get_if<mks::KeyEvent>(&keyInput->event);
    EXPECT_NE(key, nullptr);
    if (!key) {
        co_return;
    }
    EXPECT_EQ(key->key, mks::Key::A);
    EXPECT_EQ(key->modifiers, mks::KeyModifier::LeftCtrl);
    EXPECT_EQ(key->nativeCode, 30U);
    co_return;
}

ILIAS_TEST(ServerInputRouting, SwitchesBackToLocalAcrossRemoteLeftEdge) {
    auto localEndpoint = makeEndpoint(30010);
    auto remoteEndpoint = makeEndpoint(30011);
    auto server = mks::Server {localEndpoint};
    auto capture = mks::test::MockInputCapture {};
    auto [sender, receiver] = ilias::mpsc::channel<mks::RpcMessage>(10);
    server.attachCaptureForTest(&capture);

    server.registerScreensForTest(localEndpoint, {
        makeScreen("local-primary", 1920, 1080, true),
    }, true);
    server.registerScreensForTest(remoteEndpoint, {
        makeScreen("remote-primary", 2560, 1440, true),
    }, false);
    server.attachClientSenderForTest(remoteEndpoint, sender);

    auto localKey = mks::ScreenKey {
        .ownerId = fmtlib::format("{}", localEndpoint),
        .screenIndex = 0,
    };

    server.handleInputEventForTest(mks::InputEvent {mks::MouseMoveEvent {
        .x = 1919,
        .y = 540,
        .screenIndex = 0,
    }});

    auto entryMessage = co_await receiver.recv();
    EXPECT_TRUE(static_cast<bool>(entryMessage));
    if (!entryMessage) {
        co_return;
    }

    server.handleInputEventForTest(mks::InputEvent {mks::MouseMoveEvent {
        .x = 1900,
        .y = 540,
        .screenIndex = 0,
    }});

    EXPECT_TRUE(server.activeScreenKey().has_value());
    if (!server.activeScreenKey()) {
        co_return;
    }
    EXPECT_EQ(*server.activeScreenKey(), localKey);
    EXPECT_FALSE(static_cast<bool>(receiver.tryRecv()));

    auto cursorMove = capture.lastCursorMove();
    EXPECT_TRUE(cursorMove.has_value());
    if (!cursorMove) {
        co_return;
    }
    EXPECT_EQ(cursorMove->screenIndex, 0U);
    EXPECT_EQ(cursorMove->x, 1919);
    EXPECT_EQ(cursorMove->y, 540);

    server.handleInputEventForTest(mks::InputEvent {mks::MouseMoveEvent {
        .x = 1919,
        .y = 540,
        .screenIndex = 0,
    }});

    EXPECT_TRUE(server.activeScreenKey().has_value());
    if (!server.activeScreenKey()) {
        co_return;
    }
    EXPECT_EQ(*server.activeScreenKey(), localKey);
    EXPECT_FALSE(static_cast<bool>(receiver.tryRecv()));
    co_return;
}

ILIAS_TEST(ServerInputRouting, SwitchesAcrossMultipleRemoteScreens) {
    auto localEndpoint = makeEndpoint(30012);
    auto remoteEndpoint = makeEndpoint(30013);
    auto server = mks::Server {localEndpoint};
    auto [sender, receiver] = ilias::mpsc::channel<mks::RpcMessage>(10);

    server.registerScreensForTest(localEndpoint, {
        makeScreen("local-primary", 1920, 1080, true),
    }, true);
    server.registerScreensForTest(remoteEndpoint, {
        makeScreen("remote-primary", 2560, 1440, true),
        makeScreen("remote-side", 1600, 900, false),
    }, false);
    server.attachClientSenderForTest(remoteEndpoint, sender);

    auto remoteSideKey = mks::ScreenKey {
        .ownerId = fmtlib::format("{}", remoteEndpoint),
        .screenIndex = 1,
    };

    server.handleInputEventForTest(mks::InputEvent {mks::MouseMoveEvent {
        .x = 1919,
        .y = 540,
        .screenIndex = 0,
    }});

    auto entryMessage = co_await receiver.recv();
    EXPECT_TRUE(static_cast<bool>(entryMessage));
    if (!entryMessage) {
        co_return;
    }

    server.handleInputEventForTest(mks::InputEvent {mks::MouseMoveEvent {
        .x = 4478,
        .y = 540,
        .screenIndex = 0,
    }});

    auto sideMessage = co_await receiver.recv();
    EXPECT_TRUE(static_cast<bool>(sideMessage));
    if (!sideMessage) {
        co_return;
    }

    EXPECT_TRUE(server.activeScreenKey().has_value());
    if (!server.activeScreenKey()) {
        co_return;
    }
    EXPECT_EQ(*server.activeScreenKey(), remoteSideKey);

    auto *sideInput = std::get_if<mks::InputMessage>(&*sideMessage);
    EXPECT_NE(sideInput, nullptr);
    if (!sideInput) {
        co_return;
    }

    auto *sideMove = std::get_if<mks::MouseMoveEvent>(&sideInput->event);
    EXPECT_NE(sideMove, nullptr);
    if (!sideMove) {
        co_return;
    }
    EXPECT_EQ(sideMove->x, 0);
    EXPECT_EQ(sideMove->y, 450);
    EXPECT_EQ(sideMove->screenIndex, 1U);
    co_return;
}

int main(int argc, char **argv) {
    ILIAS_TEST_SETUP_UTF8();
    ilias::PlatformContext context {};
    context.install();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
