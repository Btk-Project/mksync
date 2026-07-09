#include "app/client.hpp"
#include "app/server.hpp"
#include "platform/platform.hpp"
#include "rpc/transport.hpp"
#include "support/mock_platform.hpp"

#include <gtest/gtest.h>
#include <ilias/testing.hpp>
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

auto forwardOneMessage(
    ilias::mpsc::Receiver<mks::RpcMessage> &receiver,
    mks::RpcTransport &transport
) -> mks::IoTask<void> {
    auto message = co_await receiver.recv();
    if (!message) {
        co_return mks::Err(mks::RpcError::ProtocolError);
    }
    ILIAS_CO_TRYV(co_await transport.writeMessage(*message));
    co_return {};
}

auto receiveOneMessage(
    mks::RpcTransport &transport,
    mks::Client &client,
    mks::InputInjector &injector
) -> mks::IoTask<void> {
    ILIAS_CO_TRY(auto message, co_await transport.readMessage());
    ILIAS_CO_TRYV(co_await client.handleMessageForTest(message, injector));
    co_return {};
}

auto forwardAndReceiveOneMessage(
    ilias::mpsc::Receiver<mks::RpcMessage> &receiver,
    mks::RpcTransport &serverTransport,
    mks::RpcTransport &clientTransport,
    mks::Client &client,
    mks::InputInjector &injector
) -> mks::IoTask<void> {
    auto [forward, receive] = co_await ilias::whenAll(
        forwardOneMessage(receiver, serverTransport),
        receiveOneMessage(clientTransport, client, injector)
    );
    if (!forward) {
        co_return mks::Err(forward.error());
    }
    if (!receive) {
        co_return mks::Err(receive.error());
    }
    co_return {};
}

} // namespace

ILIAS_TEST(InputPipeline, ServerMessageReachesClientInjector) {
    auto localEndpoint = makeEndpoint(30201);
    auto remoteEndpoint = makeEndpoint(30202);
    auto server = mks::Server {localEndpoint};
    auto client = mks::Client {makeEndpoint(30203)};
    auto platform = mks::test::MockPlatform {{}};
    auto injector = platform.injector();

    auto initResult = co_await injector->initialize();
    EXPECT_TRUE(initResult.has_value()) << initResult.error().message();
    if (!initResult) {
        co_return;
    }

    auto [sender, receiver] = ilias::mpsc::channel<mks::RpcMessage>(10);
    auto [serverStream, clientStream] = ilias::DuplexStream::make(4096);
    auto serverTransport = mks::RpcTransport {std::move(serverStream)};
    auto clientTransport = mks::RpcTransport {std::move(clientStream)};

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

    auto [forwardEntry, receiveEntry] = co_await ilias::whenAll(
        forwardOneMessage(receiver, serverTransport),
        receiveOneMessage(clientTransport, client, *injector)
    );
    EXPECT_TRUE(forwardEntry.has_value()) << forwardEntry.error().message();
    EXPECT_TRUE(receiveEntry.has_value()) << receiveEntry.error().message();
    if (!forwardEntry || !receiveEntry) {
        co_return;
    }

    auto events = injector->events();
    EXPECT_EQ(events.size(), 1U);
    if (events.size() != 1U) {
        co_return;
    }
    auto *entryMove = std::get_if<mks::MouseMoveEvent>(&events[0]);
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

    auto [forwardMove, receiveMove] = co_await ilias::whenAll(
        forwardOneMessage(receiver, serverTransport),
        receiveOneMessage(clientTransport, client, *injector)
    );
    EXPECT_TRUE(forwardMove.has_value()) << forwardMove.error().message();
    EXPECT_TRUE(receiveMove.has_value()) << receiveMove.error().message();
    if (!forwardMove || !receiveMove) {
        co_return;
    }

    events = injector->events();
    EXPECT_EQ(events.size(), 2U);
    if (events.size() != 2U) {
        co_return;
    }
    auto *remoteMove = std::get_if<mks::MouseMoveEvent>(&events[1]);
    EXPECT_NE(remoteMove, nullptr);
    if (!remoteMove) {
        co_return;
    }
    EXPECT_EQ(remoteMove->x, 10);
    EXPECT_EQ(remoteMove->y, 730);
    EXPECT_EQ(remoteMove->screenIndex, 0U);

    server.handleInputEventForTest(mks::InputEvent {mks::KeyEvent {
        .key = mks::Key::A,
        .modifiers = mks::KeyModifier::LeftCtrl,
        .nativeCode = 30,
        .repeat = false,
        .release = false,
    }});

    auto [forwardKey, receiveKey] = co_await ilias::whenAll(
        forwardOneMessage(receiver, serverTransport),
        receiveOneMessage(clientTransport, client, *injector)
    );
    EXPECT_TRUE(forwardKey.has_value()) << forwardKey.error().message();
    EXPECT_TRUE(receiveKey.has_value()) << receiveKey.error().message();
    if (!forwardKey || !receiveKey) {
        co_return;
    }

    events = injector->events();
    EXPECT_EQ(events.size(), 3U);
    if (events.size() != 3U) {
        co_return;
    }
    auto *key = std::get_if<mks::KeyEvent>(&events[2]);
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

ILIAS_TEST(InputPipeline, FullMockOperationCrossesRemoteReturnsLocalAndRoutesKeyboard) {
    auto localEndpoint = makeEndpoint(30211);
    auto remoteEndpoint = makeEndpoint(30212);
    auto server = mks::Server {localEndpoint};
    auto client = mks::Client {makeEndpoint(30213)};
    auto platform = mks::test::MockPlatform {{}};
    auto injector = platform.injector();

    auto initResult = co_await injector->initialize();
    EXPECT_TRUE(initResult.has_value()) << initResult.error().message();
    if (!initResult) {
        co_return;
    }

    auto [sender, receiver] = ilias::mpsc::channel<mks::RpcMessage>(10);
    auto [serverStream, clientStream] = ilias::DuplexStream::make(4096);
    auto serverTransport = mks::RpcTransport {std::move(serverStream)};
    auto clientTransport = mks::RpcTransport {std::move(clientStream)};

    server.registerScreensForTest(localEndpoint, {
        makeScreen("local-primary", 1920, 1080, true),
    }, true);
    server.registerScreensForTest(remoteEndpoint, {
        makeScreen("remote-primary", 2560, 1440, true),
    }, false);
    server.attachClientSenderForTest(remoteEndpoint, sender);

    const auto localKey = mks::ScreenKey {
        .ownerId = fmtlib::format("{}", localEndpoint),
        .screenIndex = 0,
    };
    const auto remoteKey = mks::ScreenKey {
        .ownerId = fmtlib::format("{}", remoteEndpoint),
        .screenIndex = 0,
    };

    EXPECT_TRUE(server.activeScreenKey().has_value());
    if (!server.activeScreenKey()) {
        co_return;
    }
    EXPECT_EQ(*server.activeScreenKey(), localKey);

    server.handleInputEventForTest(mks::InputEvent {mks::MouseMoveEvent {
        .x = 1919,
        .y = 540,
        .screenIndex = 0,
    }});

    auto result = co_await forwardAndReceiveOneMessage(
        receiver,
        serverTransport,
        clientTransport,
        client,
        *injector
    );
    EXPECT_TRUE(result.has_value()) << result.error().message();
    if (!result) {
        co_return;
    }

    EXPECT_EQ(server.activeScreenKey(), remoteKey);
    auto events = injector->events();
    EXPECT_EQ(events.size(), 1U);
    if (events.size() != 1U) {
        co_return;
    }
    auto *entryMove = std::get_if<mks::MouseMoveEvent>(&events[0]);
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

    result = co_await forwardAndReceiveOneMessage(
        receiver,
        serverTransport,
        clientTransport,
        client,
        *injector
    );
    EXPECT_TRUE(result.has_value()) << result.error().message();
    if (!result) {
        co_return;
    }

    events = injector->events();
    EXPECT_EQ(events.size(), 2U);
    if (events.size() != 2U) {
        co_return;
    }
    auto *remoteMove = std::get_if<mks::MouseMoveEvent>(&events[1]);
    EXPECT_NE(remoteMove, nullptr);
    if (!remoteMove) {
        co_return;
    }
    EXPECT_EQ(remoteMove->x, 10);
    EXPECT_EQ(remoteMove->y, 730);
    EXPECT_EQ(remoteMove->screenIndex, 0U);

    server.handleInputEventForTest(mks::InputEvent {mks::KeyEvent {
        .key = mks::Key::A,
        .modifiers = mks::KeyModifier::LeftCtrl,
        .nativeCode = 30,
        .repeat = false,
        .release = false,
    }});

    result = co_await forwardAndReceiveOneMessage(
        receiver,
        serverTransport,
        clientTransport,
        client,
        *injector
    );
    EXPECT_TRUE(result.has_value()) << result.error().message();
    if (!result) {
        co_return;
    }

    server.handleInputEventForTest(mks::InputEvent {mks::KeyEvent {
        .key = mks::Key::A,
        .modifiers = mks::KeyModifier::None,
        .nativeCode = 30,
        .repeat = false,
        .release = true,
    }});

    result = co_await forwardAndReceiveOneMessage(
        receiver,
        serverTransport,
        clientTransport,
        client,
        *injector
    );
    EXPECT_TRUE(result.has_value()) << result.error().message();
    if (!result) {
        co_return;
    }

    events = injector->events();
    EXPECT_EQ(events.size(), 4U);
    if (events.size() != 4U) {
        co_return;
    }
    auto *keyDown = std::get_if<mks::KeyEvent>(&events[2]);
    EXPECT_NE(keyDown, nullptr);
    if (!keyDown) {
        co_return;
    }
    EXPECT_EQ(keyDown->key, mks::Key::A);
    EXPECT_EQ(keyDown->modifiers, mks::KeyModifier::LeftCtrl);
    EXPECT_EQ(keyDown->nativeCode, 30U);
    EXPECT_FALSE(keyDown->release);

    auto *keyUp = std::get_if<mks::KeyEvent>(&events[3]);
    EXPECT_NE(keyUp, nullptr);
    if (!keyUp) {
        co_return;
    }
    EXPECT_EQ(keyUp->key, mks::Key::A);
    EXPECT_EQ(keyUp->nativeCode, 30U);
    EXPECT_TRUE(keyUp->release);

    server.handleInputEventForTest(mks::InputEvent {mks::MouseMoveEvent {
        .x = 1900,
        .y = 550,
        .screenIndex = 0,
    }});

    EXPECT_TRUE(server.activeScreenKey().has_value());
    if (!server.activeScreenKey()) {
        co_return;
    }
    EXPECT_EQ(*server.activeScreenKey(), localKey);
    EXPECT_FALSE(static_cast<bool>(receiver.tryRecv()));
    EXPECT_EQ(injector->events().size(), 4U);

    server.handleInputEventForTest(mks::InputEvent {mks::KeyEvent {
        .key = mks::Key::B,
        .modifiers = mks::KeyModifier::None,
        .nativeCode = 31,
        .repeat = false,
        .release = false,
    }});

    EXPECT_FALSE(static_cast<bool>(receiver.tryRecv()));
    EXPECT_EQ(injector->events().size(), 4U);

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
