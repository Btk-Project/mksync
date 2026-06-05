#include "preinclude.hpp"
#include "rpc/message.hpp"
#include "rpc/transport.hpp"

#include <gtest/gtest.h>
#include <ilias/testing.hpp>

namespace {

auto clientRoundTrip(mks::RpcTransport &client) -> mks::IoTask<void> {
    ILIAS_CO_TRYV(co_await client.writeMessage(mks::RpcMessage {mks::HelloMessage {
        .version = 1,
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
    EXPECT_EQ(hello.name, "transport-test-client");

    ILIAS_CO_TRYV(co_await server.writeMessage(mks::RpcMessage {mks::PongMessage {}}));
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

int main(int argc, char **argv) {
    ILIAS_TEST_SETUP_UTF8();
    ilias::PlatformContext context {};
    context.install();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
