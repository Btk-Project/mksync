#include "support/mock_platform.hpp"

#include <gtest/gtest.h>
#include <ilias/testing.hpp>

namespace
{

    auto makeScreen(std::string name, int32_t width, int32_t height, bool primary)
        -> mks::ScreenInfo
    {
        return mks::ScreenInfo{
            .x       = 0,
            .y       = 0,
            .width   = width,
            .height  = height,
            .dpi     = 72,
            .name    = std::move(name),
            .primary = primary,
        };
    }

} // namespace

using namespace std::chrono_literals;
using namespace mks::test::literals;

TEST(MockPlatform, ReturnsConfiguredScreens)
{
    auto platform = mks::test::MockPlatform{
        {
         makeScreen("primary", 1920, 1080, true),
         makeScreen("side", 1280, 720, false),
         }
    };

    auto screens = platform.screens();
    ASSERT_EQ(screens.size(), 2U);
    EXPECT_EQ(screens[0].name, "primary");
    EXPECT_EQ(screens[0].width, 1920);
    EXPECT_TRUE(screens[0].primary);
    EXPECT_EQ(screens[1].name, "side");
}

ILIAS_TEST(MockPlatform, CaptureCanPushAndReceiveEvents)
{
    auto platform = mks::test::MockPlatform{
        {
         makeScreen("primary", 1920, 1080, true),
         }
    };
    auto capture = platform.capture();

    auto initResult = co_await capture->initialize();
    EXPECT_TRUE(initResult.has_value()) << initResult.error().message();
    if (!initResult) {
        co_return;
    }

    EXPECT_TRUE(capture->push(mks::InputEvent{
        mks::MouseMoveEvent{
                            .x           = 120,
                            .y           = 240,
                            .screenIndex = 0,
                            }
    }));

    auto  event = co_await capture->nextEvent();
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

TEST(MockPlatform, CaptureRecordsLocalCursorMoves)
{
    auto platform = mks::test::MockPlatform{
        {
         makeScreen("primary", 1920, 1080, true),
         }
    };
    auto capture = platform.capture();

    auto result = capture->moveLocalCursor(0, 640, 360);
    ASSERT_TRUE(result.has_value()) << result.error().message();

    auto move = capture->lastCursorMove();
    ASSERT_TRUE(move.has_value());
    EXPECT_EQ(move->screenIndex, 0U);
    EXPECT_EQ(move->x, 640);
    EXPECT_EQ(move->y, 360);
}

ILIAS_TEST(MockPlatform, InjectorRecordsInjectedEvents)
{
    auto platform = mks::test::MockPlatform{
        {
         makeScreen("primary", 1920, 1080, true),
         }
    };
    auto injector = platform.injector();

    auto initResult = co_await injector->initialize();
    EXPECT_TRUE(initResult.has_value()) << initResult.error().message();
    if (!initResult) {
        co_return;
    }

    auto injectResult = co_await injector->inject(mks::InputEvent{
        mks::KeyEvent{
                      .key        = mks::Key::A,
                      .modifiers  = mks::KeyModifier::LeftCtrl,
                      .nativeCode = 30,
                      .repeat     = false,
                      .release    = false,
                      }
    });
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

ILIAS_TEST(MockPlatformDsl, PlaysTimedInterpolationAndClicksInOrder)
{
    auto platform = mks::test::MockPlatform{
        {
         makeScreen("primary", 1920, 1080, true),
         }
    };
    auto capture     = platform.capture();
    auto initialized = co_await capture->initialize();
    EXPECT_TRUE(initialized.has_value()) << initialized.error().message();
    if (!initialized) {
        co_return;
    }

    platform.actions()
        .frequency(250_hz)
        .mouse()
        .set(0, 0)
        .interpolationMove(10, 6, 16ms, 125_hz)
        .click(mks::MouseButton::Right)
        .keyboard()
        .click(mks::Key::A, mks::KeyModifier::LeftCtrl, 30);

    EXPECT_TRUE(co_await platform.play());
    auto events = std::vector<mks::InputEvent>{};
    for (auto index = 0U; index < 7U; ++index) {
        events.push_back(co_await capture->nextEvent());
    }

    auto *firstStep = std::get_if<mks::MouseMoveEvent>(&events[1]);
    auto *lastStep  = std::get_if<mks::MouseMoveEvent>(&events[2]);
    EXPECT_NE(firstStep, nullptr);
    EXPECT_NE(lastStep, nullptr);
    if (firstStep && lastStep) {
        EXPECT_EQ(firstStep->x, 5);
        EXPECT_EQ(firstStep->y, 3);
        EXPECT_EQ(firstStep->deltaX, 5);
        EXPECT_EQ(firstStep->deltaY, 3);
        EXPECT_EQ(lastStep->x, 10);
        EXPECT_EQ(lastStep->y, 6);
    }
    auto *buttonDown = std::get_if<mks::MouseButtonEvent>(&events[3]);
    auto *buttonUp   = std::get_if<mks::MouseButtonEvent>(&events[4]);
    auto *keyDown    = std::get_if<mks::KeyEvent>(&events[5]);
    auto *keyUp      = std::get_if<mks::KeyEvent>(&events[6]);
    EXPECT_NE(buttonDown, nullptr);
    EXPECT_NE(buttonUp, nullptr);
    EXPECT_NE(keyDown, nullptr);
    EXPECT_NE(keyUp, nullptr);
    if (!buttonDown || !buttonUp || !keyDown || !keyUp) {
        co_return;
    }
    EXPECT_FALSE(buttonDown->release);
    EXPECT_TRUE(buttonUp->release);
    EXPECT_FALSE(keyDown->release);
    EXPECT_TRUE(keyUp->release);

    co_await capture->shutdown();
    co_return;
}

ILIAS_TEST(MockPlatformDsl, VerifiesExpectedDeviceSequence)
{
    auto platform = mks::test::MockPlatform{
        {
         makeScreen("primary", 1920, 1080, true),
         }
    };
    auto injector    = platform.injector();
    auto initialized = co_await injector->initialize();
    EXPECT_TRUE(initialized.has_value()) << initialized.error().message();
    if (!initialized) {
        co_return;
    }

    platform.expect()
        .mouse()
        .set(20, 30)
        .moveBy(4, -2)
        .scroll(0, -120)
        .doubleClick(mks::MouseButton::Left)
        .keyboard()
        .chord(mks::KeyModifier::LeftCtrl, mks::Key::A, 30);
    for (const auto &event : platform.expect().events()) {
        auto injected = co_await injector->inject(event);
        EXPECT_TRUE(injected.has_value()) << injected.error().message();
        if (!injected) {
            co_return;
        }
    }

    auto verification = platform.verify();
    EXPECT_TRUE(static_cast<bool>(verification)) << verification.description;
    co_await injector->shutdown();
    co_return;
}

int main(int argc, char **argv)
{
    ILIAS_TEST_SETUP_UTF8();
    ilias::PlatformContext context{};
    context.install();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
