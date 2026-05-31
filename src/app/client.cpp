#include "platform/platform.hpp"
#include "rpc/transport.hpp"
#include "rpc/message.hpp"
#include "client.hpp"

MKS_BEGIN

Client::Client(IPEndpoint endpoint) : mEndpoint(endpoint) {}

auto Client::run() -> IoTask<void> {
    // Create the injector backend
    auto platform = Platform::create();
    assert(platform);
    
    SPDLOG_INFO("Connecting to server to {}", mEndpoint);
    ILIAS_CO_TRY(auto stream, co_await TcpStream::connect(mEndpoint));

    // Send Hello to it
    char computerNameBuf[256];
    auto computerNameLen = ::gethostname(computerNameBuf, sizeof(computerNameBuf));
    assert(computerNameLen != -1);
    std::string_view computerName {computerNameBuf, static_cast<size_t>(computerNameLen)};
    SPDLOG_INFO("Computer name: {}", computerName);

    RpcTransport transport {std::move(stream)};
    ILIAS_CO_TRYV(co_await transport.writeMessage(RpcMessage {HelloMessage {
        .version = 0,
        .name = std::string {computerName},
    }}));

    // Start the reader part and the writer part
    co_await ilias::whenAny(
        handleRead(&transport),
        handleWrite(platform.get(), &transport)
    );
    co_return {};
}

auto Client::handleWrite(void *_platform, void *_transport) -> IoTask<void> {
    using namespace std::literals;
    auto platform = static_cast<Platform*>(_platform);
    auto transport = static_cast<RpcTransport*>(_transport);

    // First, send screen to it
    ILIAS_CO_TRYV(co_await transport->writeMessage(RpcMessage {ScreensMessage {
        .screens = platform->screens(),
    }}));

    // TODO: 
    while (true) {
        co_await ilias::sleep(1s);
        // ILIAS_CO_TRYV(co_await transport->writeMessage(RpcMessage {PingMessage {} }));
    }
}

auto Client::handleRead(void *_transport) -> IoTask<void> {
    auto transport = static_cast<RpcTransport*>(_transport);
    while (true) {
        ILIAS_CO_TRY(auto msg, co_await transport->readMessage());
        SPDLOG_INFO("Received message: {}", msg);
    }
}


MKS_END