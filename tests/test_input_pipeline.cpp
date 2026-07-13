#include "app/client.hpp"
#include "app/server.hpp"
#include "platform/platform.hpp"
#include "support/mock_platform.hpp"

#include <chrono>
#include <gtest/gtest.h>
#include <ilias/testing.hpp>
#include <memory>
#include <stdexcept>
#include <vector>

auto mks::Platform::create() -> Ptr
{
    return nullptr;
}

namespace
{

    using namespace std::chrono_literals;
    using namespace mks::test::literals;

    auto makeEndpoint(uint16_t port) -> mks::IPEndpoint
    {
        auto endpoint = mks::IPEndpoint::fromString(fmtlib::format("127.0.0.1:{}", port));
        if (!endpoint) {
            throw std::runtime_error("invalid test endpoint");
        }
        return *endpoint;
    }

    auto makeScreen(std::string name, int32_t width, int32_t height) -> mks::ScreenInfo
    {
        return mks::ScreenInfo{
            .x       = 0,
            .y       = 0,
            .width   = width,
            .height  = height,
            .dpi     = 72,
            .name    = std::move(name),
            .primary = true,
        };
    }

    template <typename Predicate>
    auto waitUntil(Predicate predicate, std::chrono::milliseconds timeout) -> mks::Task<bool>
    {
        constexpr auto interval = 5ms;
        for (auto elapsed = 0ms; elapsed < timeout; elapsed += interval) {
            if (predicate()) {
                co_return true;
            }
            co_await ilias::sleep(interval);
        }
        co_return predicate();
    }

} // namespace

ILIAS_TEST(InputPipelineLoopback, CompleteMouseAndKeyboardFlowReachesClientInjector)
{
    auto endpoint = makeEndpoint(30201);
    auto serverPlatform =
        std::make_shared<mks::test::MockPlatform>(std::vector{makeScreen("server", 1920, 1080)});
    auto clientPlatform =
        std::make_shared<mks::test::MockPlatform>(std::vector{makeScreen("client", 2560, 1440)});
    auto server = mks::Server{serverPlatform, endpoint};
    auto client = mks::Client{clientPlatform, endpoint};

    serverPlatform->actions()
        .frequency(250_hz)
        .mouse()
        .set(1919, 540)
        .sleep(16ms)
        .interpolationMove(1929, 550, 32ms, 125_hz)
        .press(mks::MouseButton::Right)
        .release(mks::MouseButton::Right)
        .click(mks::MouseButton::Right)
        .keyboard()
        .click(mks::Key::A, mks::KeyModifier::LeftCtrl, 30)
        .repeat(mks::Key::B, 16ms, 125_hz, mks::KeyModifier::None, 31);
    clientPlatform->expect()
        .mouse()
        .set(0, 720)
        .interpolationMove(10, 730, 32ms, 125_hz)
        .press(mks::MouseButton::Right)
        .release(mks::MouseButton::Right)
        .click(mks::MouseButton::Right)
        .keyboard()
        .click(mks::Key::A, mks::KeyModifier::LeftCtrl, 30)
        .repeat(mks::Key::B, 16ms, 125_hz, mks::KeyModifier::None, 31);

    auto runClient = [&]() -> mks::IoTask<void> {
        co_await ilias::sleep(20ms);
        co_return co_await client.run();
    };

    auto exercisePipeline = [&]() -> mks::Task<bool> {
        auto connected =
            co_await waitUntil([&] { return server.topologyScreens().size() == 2; }, 1s);
        if (!connected) {
            co_return false;
        }

        if (!co_await serverPlatform->play()) {
            co_return false;
        }
        if (!co_await clientPlatform->waitForExpected(1s)) {
            co_return false;
        }
        co_return serverPlatform->capture()->remoteControlActive() &&
                  server.activeScreenKey().has_value();
    };

    auto [serverResult, clientResult, exercised] =
        co_await ilias::whenAny(server.run(), runClient(), exercisePipeline());

    EXPECT_TRUE(exercised.has_value())
        << "server or client stopped before the loopback input flow completed";
    if (!exercised) {
        co_return;
    }
    EXPECT_TRUE(*exercised) << "loopback input flow timed out";
    if (!*exercised) {
        co_return;
    }
    EXPECT_FALSE(serverResult.has_value());
    EXPECT_FALSE(clientResult.has_value());

    auto verification = clientPlatform->verify();
    EXPECT_TRUE(static_cast<bool>(verification)) << verification.description;
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
