#include "rpc/transport.hpp"
#include "rpc/message.hpp"
#include "client.hpp"
#include <cassert>
#include <cerrno>
#include <cstring>
#include <utility>

MKS_BEGIN

Client::Client(Platform::Ptr platform, IPEndpoint endpoint)
    : Client(std::move(platform), endpoint, AppConfig {}) {

}

Client::Client(Platform::Ptr platform, IPEndpoint endpoint, AppConfig config)
    : mPlatform(std::move(platform)),
      mEndpoint(endpoint),
      mConfig(std::move(config)) {
    // Interface invariant: callers inject a live Platform.
    assert(mPlatform);
    ensureMachineId(mConfig);
}

auto Client::run() -> IoTask<void> {
    auto injector = mPlatform->createInjector();
    if (!injector) {
        SPDLOG_ERROR("Current platform does not provide an input injector");
        co_return Err(std::make_error_code(std::errc::operation_not_supported));
    }
    
    SPDLOG_INFO("Connecting to server to {}", mEndpoint);
    ILIAS_CO_TRY(auto stream, co_await TcpStream::connect(mEndpoint));

    // Send Hello to it
    char computerNameBuf[256] {};
    if (::gethostname(computerNameBuf, sizeof(computerNameBuf)) == -1) {
        co_return Err(std::error_code(errno, std::generic_category()));
    }
    computerNameBuf[sizeof(computerNameBuf) - 1] = '\0';
    auto computerNameLen = std::strlen(computerNameBuf);
    std::string_view computerName {computerNameBuf, static_cast<size_t>(computerNameLen)};
    SPDLOG_INFO("Computer name: {}", computerName);

    RpcTransport transport {std::move(stream)};
    ILIAS_CO_TRYV(co_await transport.writeMessage(RpcMessage {HelloMessage {
        .version = 0,
        .machineId = mConfig.machineId,
        .name = std::string {computerName},
    }}));

    // Initialize injection only after the server accepts the identity. If this
    // fails, the connection exits before advertising screens that cannot accept
    // forwarded input.
    ILIAS_CO_TRYV(co_await injector->initialize());

    // Start the reader part and the writer part
    auto [readResult, writeResult] = co_await ilias::finally(
        ilias::whenAny(
            handleRead(transport, *injector),
            handleWrite(transport)
        ),
        shutdownConnection(transport, *injector)
    );

    if (readResult) {
        ILIAS_CO_TRYV(std::move(*readResult));
    }
    if (writeResult) {
        ILIAS_CO_TRYV(std::move(*writeResult));
    }
    co_return {};
}

auto Client::shutdownConnection(RpcTransport &transport, InputInjector &injector) -> Task<void> {
    SPDLOG_INFO("Client shutting down connection to {}", mEndpoint);
    auto result = co_await transport.shutdown();
    if (!result) {
        SPDLOG_WARN(
            "Client transport shutdown failed for {}: {}",
            mEndpoint,
            result.error().message()
        );
    }
    transport.close();
    co_await injector.shutdown();
    SPDLOG_INFO("Client connection shutdown complete for {}", mEndpoint);
    co_return;
}

auto Client::handleWrite(RpcTransport &transport) -> IoTask<void> {
    using namespace std::literals;

    // First, send screen to it. These coordinates are the client's own real
    // screen rects; the server stores them for entry-point mapping and sends
    // input back in the same screenIndex/x/y space.
    ILIAS_CO_TRYV(co_await transport.writeMessage(RpcMessage {ScreensMessage {
        .screens = mPlatform->screens(),
    }}));

    // TODO: 
    while (true) {
        co_await ilias::sleep(1s);
        // ILIAS_CO_TRYV(co_await transport.writeMessage(RpcMessage {PingMessage {} }));
    }
}

auto Client::handleRead(RpcTransport &transport, InputInjector &injector) -> IoTask<void> {
    while (true) {
        ILIAS_CO_TRY(auto msg, co_await transport.readMessage());
        SPDLOG_TRACE("Client received message {}", msg);
        ILIAS_CO_TRYV(co_await handleMessage(msg, injector));
    }
}

auto Client::handleMessage(const RpcMessage &message, InputInjector &injector) -> IoTask<void> {
    if (auto input = std::get_if<InputMessage>(&message)) {
        // InputMessage already carries target-client coordinates. The client
        // side should inject directly instead of re-running topology logic.
        SPDLOG_TRACE("Client injecting input event {}", input->event);
        auto injected = co_await injector.inject(input->event);
        if (!injected) {
            SPDLOG_WARN(
                "Client failed to inject input event {}: {}",
                input->event,
                injected.error().message()
            );
            co_return Err(injected.error());
        }

        if (const auto *move = std::get_if<MouseMoveEvent>(&input->event)) {
            if (!mLastInjectedMouseScreen || *mLastInjectedMouseScreen != move->screenIndex) {
                SPDLOG_INFO(
                    "Client cursor entered local screen={} at ({}, {})",
                    move->screenIndex,
                    move->x,
                    move->y
                );
            }
            else {
                SPDLOG_TRACE(
                    "Client cursor moved on local screen={} to ({}, {})",
                    move->screenIndex,
                    move->x,
                    move->y
                );
            }
            mLastInjectedMouseScreen = move->screenIndex;
        }
        SPDLOG_TRACE("Client injected input event {}", input->event);
        co_return {};
    }

    SPDLOG_TRACE("Client received non-input message {}", message);
    co_return {};
}

MKS_END
