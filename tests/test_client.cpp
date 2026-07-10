#include "app/client.hpp"
#include "platform/platform.hpp"
#include "rpc/message.hpp"
#include "support/mock_platform.hpp"

#include <gtest/gtest.h>
#include <ilias/testing.hpp>
#include <memory>
#include <stdexcept>
#include <vector>

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

} // namespace

ILIAS_TEST(ClientInput, InputMessageInjectsEvent) {
    auto platform = std::make_shared<mks::test::MockPlatform>(std::vector<mks::ScreenInfo> {});
    auto injector = platform->injector();

    auto initResult = co_await injector->initialize();
    EXPECT_TRUE(initResult.has_value()) << initResult.error().message();
    if (!initResult) {
        co_return;
    }

    auto client = mks::Client {platform, makeEndpoint(30101)};
    auto result = co_await client.handleMessageForTest(
        mks::RpcMessage {mks::InputMessage {
            .event = mks::InputEvent {mks::KeyEvent {
                .key = mks::Key::A,
                .modifiers = mks::KeyModifier::LeftCtrl,
                .nativeCode = 30,
                .repeat = false,
                .release = false,
            }},
        }},
        *injector
    );

    EXPECT_TRUE(result.has_value()) << result.error().message();
    if (!result) {
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
