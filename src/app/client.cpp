#include "platform/platform.hpp"
#include "rpc/transport.hpp"
#include "rpc/message.hpp"
#include "client.hpp"

MKS_BEGIN

Client::Client(IPEndpoint endpoint) : Client(endpoint, AppConfig {}) {

}

Client::Client(IPEndpoint endpoint, AppConfig config)
    : mEndpoint(endpoint),
      mConfig(std::move(config)) {
    ensureMachineId(mConfig);
}

auto Client::run() -> IoTask<void> {
    auto platform = Platform::create();
    assert(platform);

    auto injector = platform->createInjector();
    if (!injector) {
        SPDLOG_ERROR("Current platform does not provide an input injector");
        co_return Err(std::make_error_code(std::errc::operation_not_supported));
    }
    
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
        .machineId = mConfig.machineId,
        .name = std::string {computerName},
    }}));

    ILIAS_CO_TRYV(co_await injector->initialize());

    // Start the reader part and the writer part
    co_await ilias::finally(
        ilias::whenAny(
            handleRead(&transport, injector.get()),
            handleWrite(platform.get(), &transport)
        ),
        injector->shutdown()
    );
    co_return {};
}

#ifdef MKS_ENABLE_TEST_HOOKS
auto Client::handleMessageForTest(const RpcMessage &message, InputInjector &injector) -> IoTask<void> {
    ILIAS_CO_TRYV(co_await handleMessage(message, injector));
    co_return {};
}
#endif

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

auto Client::handleRead(void *_transport, void *_injector) -> IoTask<void> {
    auto transport = static_cast<RpcTransport*>(_transport);
    auto injector = static_cast<InputInjector*>(_injector);
    while (true) {
        ILIAS_CO_TRY(auto msg, co_await transport->readMessage());
        ILIAS_CO_TRYV(co_await handleMessage(msg, *injector));
    }
}

auto Client::handleMessage(const RpcMessage &message, InputInjector &injector) -> IoTask<void> {
    if (auto input = std::get_if<InputMessage>(&message)) {
        ILIAS_CO_TRYV(co_await injector.inject(input->event));
        co_return {};
    }

    SPDLOG_INFO("Received message: {}", message);
    co_return {};
}

MKS_END
