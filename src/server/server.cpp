#include <spdlog/spdlog.h>
#include <ilias/task.hpp>
#include "../proto/message.hpp"
#include "server.hpp"


namespace mksync::server {

Server::Server() {
    mPlatform = platform::createPlatform();
    mInputCapture = mPlatform->createInputCapture();
}

Server::~Server() {
    
}

auto Server::main() -> IoTask<void> {
    auto _ = co_await ilias::whenAll(
        collectEvents(),
        networkAccept()
    );
    co_return {};
}

auto Server::collectEvents() -> IoTask<void> {
    co_return {};
}

auto Server::networkAccept() -> IoTask<void> {
    auto listener = co_await ilias::TcpListener::bind(mBind);
    if (!listener) {
        co_return Err(listener.error());
    }
    co_await ilias::TaskScope::enter([&](auto &scope) -> Task<void> {
        while (true) {
            auto conn = co_await listener->accept();
            if (!conn) {
                SPDLOG_WARN("Failed to accept connection: {}", conn.error().message());
                continue;
            }
            auto &[stream, endpoint] = *conn;
            scope.spawn(networkHandle(std::move(stream)));
        }
    });
    co_return {};
}

auto Server::networkHandle(ilias::TcpStream tcp) -> Task<void> {
    SPDLOG_INFO("New connection from {}", tcp.remoteEndpoint().value());
    auto stream = ilias::BufStream { std::move(tcp) };
    // TODO:...
    while (true) {

    }
    co_return;
}

} // namespace mksync::server