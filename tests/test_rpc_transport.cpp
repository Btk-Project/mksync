#include "preinclude.hpp"
#include "rpc/message.hpp"
#include "rpc/transport.hpp"

#include <gtest/gtest.h>
#include <ilias/testing.hpp>

namespace {

auto clientRoundTrip(mks::RpcTransport &client) -> mks::IoTask<void> {
    ILIAS_CO_TRYV(co_await client.writeMessage(mks::RpcMessage {mks::HelloMessage {
        .version = 1,
        .machineId = "machine-transport-client",
        .name = "transport-test-client",
    }}));

    ILIAS_CO_TRY(auto reply, co_await client.readMessage());
    EXPECT_TRUE(std::holds_alternative<mks::PongMessage>(reply));

    co_return {};
}

auto serverRoundTrip(mks::RpcTransport &server) -> mks::IoTask<void> {
    ILIAS_CO_TRY(auto request, co_await server.readMessage());
    if (!std::holds_alternative<mks::HelloMessage>(request)) {
        ADD_FAILURE() << "server expected HelloMessage";
        co_return mks::Err(mks::RpcError::ProtocolError);
    }

    const auto &hello = std::get<mks::HelloMessage>(request);
    EXPECT_EQ(hello.version, 1);
    EXPECT_EQ(hello.machineId, "machine-transport-client");
    EXPECT_EQ(hello.name, "transport-test-client");

    ILIAS_CO_TRYV(co_await server.writeMessage(mks::RpcMessage {mks::PongMessage {}}));
    co_return {};
}

auto inputSenderRoundTrip(mks::RpcTransport &sender) -> mks::IoTask<void> {
    ILIAS_CO_TRYV(co_await sender.writeMessage(mks::RpcMessage {mks::InputMessage {
        .event = mks::InputEvent {mks::MouseMoveEvent {
            .x = 320,
            .y = 240,
            .screenIndex = 1,
        }},
    }}));
    co_return {};
}

auto inputReceiverRoundTrip(mks::RpcTransport &receiver) -> mks::IoTask<void> {
    ILIAS_CO_TRY(auto request, co_await receiver.readMessage());
    if (!std::holds_alternative<mks::InputMessage>(request)) {
        ADD_FAILURE() << "receiver expected InputMessage";
        co_return mks::Err(mks::RpcError::ProtocolError);
    }

    const auto &input = std::get<mks::InputMessage>(request);
    if (!std::holds_alternative<mks::MouseMoveEvent>(input.event)) {
        ADD_FAILURE() << "receiver expected MouseMoveEvent";
        co_return mks::Err(mks::RpcError::ProtocolError);
    }

    const auto &move = std::get<mks::MouseMoveEvent>(input.event);
    EXPECT_EQ(move.x, 320);
    EXPECT_EQ(move.y, 240);
    EXPECT_EQ(move.screenIndex, 1U);
    co_return {};
}

} // namespace

ILIAS_TEST(RpcTransport, ConnectedDuplexStreamRoundTrip) {
    auto [clientStream, serverStream] = ilias::DuplexStream::make(1024);
    mks::RpcTransport client {std::move(clientStream)};
    mks::RpcTransport server {std::move(serverStream)};

    auto [clientResult, serverResult] = co_await ilias::whenAll(
        clientRoundTrip(client),
        serverRoundTrip(server)
    );

    EXPECT_TRUE(clientResult.has_value()) << clientResult.error().message();
    EXPECT_TRUE(serverResult.has_value()) << serverResult.error().message();
}

ILIAS_TEST(RpcTransport, InputMessageRoundTrip) {
    auto [clientStream, serverStream] = ilias::DuplexStream::make(1024);
    mks::RpcTransport client {std::move(clientStream)};
    mks::RpcTransport server {std::move(serverStream)};

    auto [clientResult, serverResult] = co_await ilias::whenAll(
        inputSenderRoundTrip(client),
        inputReceiverRoundTrip(server)
    );

    EXPECT_TRUE(clientResult.has_value()) << clientResult.error().message();
    EXPECT_TRUE(serverResult.has_value()) << serverResult.error().message();
}

TEST(RpcMessage, InputMessageFormats) {
    auto text = fmtlib::format("{}", mks::RpcMessage {mks::InputMessage {
        .event = mks::InputEvent {mks::MouseMoveEvent {
            .x = 1,
            .y = 2,
            .screenIndex = 3,
        }},
    }});

    EXPECT_NE(text.find("RpcMessage"), std::string::npos);
    EXPECT_NE(text.find("InputMessage"), std::string::npos);
    EXPECT_NE(text.find("MouseMoveEvent"), std::string::npos);
}

int main(int argc, char **argv) {
    ILIAS_TEST_SETUP_UTF8();
    ilias::PlatformContext context {};
    context.install();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
