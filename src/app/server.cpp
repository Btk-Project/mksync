#include "platform/platform.hpp"
#include "rpc/message.hpp"
#include "server.hpp"

MKS_BEGIN

using ilias::TaskScope;

Server::Server(IPEndpoint endpoint) : mEndpoint(endpoint) {

}

Server::~Server() {
    
}

auto Server::run() -> IoTask<void> {
    // Create an endpoint for the server to listen on
    ILIAS_CO_TRY(auto listener, co_await TcpListener::bind(mEndpoint));
    SPDLOG_INFO("Server listening on {}", listener.localEndpoint().value());

    // Create the capture backend
    auto platform = Platform::create();
    assert(platform);
    auto capture = platform->createCapture();
    assert(capture);
    ILIAS_CO_TRYV(co_await capture->initialize());

    // Start necessary tasks
    co_await ilias::finally(
        ilias::whenAll(
            acceptIncomingConnections(std::move(listener)),
            waitPlatformEvent(capture.get())
        ),
        capture->shutdown()
    );
    co_return {};
}

auto Server::acceptIncomingConnections(TcpListener listener) -> Task<void> {
    co_return co_await TaskScope::enter([&](auto &scope) -> Task<void> {
        co_return;
    });
}

auto Server::waitPlatformEvent(void *_capture) -> Task<void> {
    SPDLOG_INFO("Server waiting for platform events");
    auto capture = static_cast<InputCapture *>(_capture);
    while (true) {
        auto event = co_await capture->nextEvent();
    }
}

MKS_END