#include "support/mock_platform.hpp"

#include <gtest/gtest.h>
#include <ilias/testing.hpp>

namespace {

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

} // namespace

TEST(MockPlatform, ReturnsConfiguredScreens) {
    auto platform = mks::test::MockPlatform {{
        makeScreen("primary", 1920, 1080, true),
        makeScreen("side", 1280, 720, false),
    }};

    auto screens = platform.screens();
    ASSERT_EQ(screens.size(), 2U);
    EXPECT_EQ(screens[0].name, "primary");
    EXPECT_EQ(screens[0].width, 1920);
    EXPECT_TRUE(screens[0].primary);
    EXPECT_EQ(screens[1].name, "side");
}

ILIAS_TEST(MockPlatform, CaptureCanPushAndReceiveEvents) {
    auto platform = mks::test::MockPlatform {{
        makeScreen("primary", 1920, 1080, true),
    }};
    auto capture = platform.capture();

    auto initResult = co_await capture->initialize();
    EXPECT_TRUE(initResult.has_value()) << initResult.error().message();
    if (!initResult) {
        co_return;
    }

    EXPECT_TRUE(capture->push(mks::InputEvent {mks::MouseMoveEvent {
        .x = 120,
        .y = 240,
        .screenIndex = 0,
    }}));

    auto event = co_await capture->nextEvent();
    auto *mouse = std::get_if<mks::MouseMoveEvent>(&event);
    EXPECT_NE(mouse, nullptr);
    if (!mouse) {
        co_return;
    }
    EXPECT_EQ(mouse->x, 120);
    EXPECT_EQ(mouse->y, 240);
    EXPECT_EQ(mouse->screenIndex, 0U);

    co_await capture->shutdown();
    co_return;
}

ILIAS_TEST(MockPlatform, InjectorRecordsInjectedEvents) {
    auto platform = mks::test::MockPlatform {{
        makeScreen("primary", 1920, 1080, true),
    }};
    auto injector = platform.injector();

    auto initResult = co_await injector->initialize();
    EXPECT_TRUE(initResult.has_value()) << initResult.error().message();
    if (!initResult) {
        co_return;
    }

    auto injectResult = co_await injector->inject(mks::InputEvent {mks::KeyEvent {
        .key = mks::Key::A,
        .modifiers = mks::KeyModifier::LeftCtrl,
        .nativeCode = 30,
        .repeat = false,
        .release = false,
    }});
    EXPECT_TRUE(injectResult.has_value()) << injectResult.error().message();
    if (!injectResult) {
        co_return;
    }

    auto events = injector->events();
    EXPECT_EQ(events.size(), 1U);
    if (events.size() != 1U) {
        co_return;
    }
    auto *key = std::get_if<mks::KeyEvent>(&events[0]);
    EXPECT_NE(key, nullptr);
    if (!key) {
        co_return;
    }
    EXPECT_EQ(key->key, mks::Key::A);
    EXPECT_EQ(key->modifiers, mks::KeyModifier::LeftCtrl);
    EXPECT_EQ(key->nativeCode, 30U);

    co_await injector->shutdown();
    co_return;
}

int main(int argc, char **argv) {
    ILIAS_TEST_SETUP_UTF8();
    ilias::PlatformContext context {};
    context.install();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
