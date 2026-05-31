#include "platform/platform.hpp"
#include "rpc/transport.hpp"
#include "rpc/message.hpp"
#include "server.hpp"
#include <ilias/sync.hpp>

MKS_BEGIN

using ilias::TaskScope;

struct ClientState {
    RpcTransport                    transport;
    ilias::mpsc::Sender<RpcMessage> sender; // Use this to send messages to the client
};

Server::Server(IPEndpoint endpoint) : mEndpoint(endpoint) {

}

Server::~Server() {
    
}

auto Server::run() -> IoTask<void> {
    // Create an endpoint for the server to listen on
    ILIAS_CO_TRY(auto listener, co_await TcpListener::bind(mEndpoint));
    ILIAS_CO_TRY(auto localEndpoint, listener.localEndpoint());
    SPDLOG_INFO("Server listening on {}", localEndpoint);

    // Create the capture backend
    auto platform = Platform::create();
    assert(platform);
    auto capture = platform->createCapture();
    assert(capture);
    ILIAS_CO_TRYV(co_await capture->initialize());

    // Register screens
    for (auto &info : platform->screens()) {
        auto screen = addScreen(localEndpoint, info);
        screen->local = true;
        if (info.primary) {
            mActiveScreen = screen;
        }
    }

    // Start necessary tasks
    co_await ilias::finally(
        ilias::whenAll(
            acceptIncomingConnections(std::move(listener)),
            waitPlatformEvent(platform.get(), capture.get())
        ),
        capture->shutdown()
    );
    co_return {};
}

auto Server::acceptIncomingConnections(TcpListener listener) -> Task<void> {
    co_return co_await TaskScope::enter([&](auto &scope) -> Task<void> {
        while (true) {
            auto imcoming = co_await listener.accept();
            if (!imcoming) {
                SPDLOG_ERROR("Server failed to accept incoming connection");
                co_return;
            }
            auto &[sock, endpoint] = *imcoming;
            scope.spawn(handleIncoming(std::move(sock)));
        }
    });
}

auto Server::waitPlatformEvent(void *_platform, void *_capture) -> Task<void> {
    SPDLOG_INFO("Server waiting for platform events");
    auto capture = static_cast<InputCapture *>(_capture);
    auto platform = static_cast<Platform *>(_platform);
    auto localScreens = platform->screens();

    // Process....
    while (true) {
        auto event = co_await capture->nextEvent();
        // SPDLOG_INFO("{}", event);
        if (auto key = std::get_if<KeyEvent>(&event)) {
            if (key->key == Key::F12 && !key->release) {
                SPDLOG_INFO("Server F12 pressed");
            }
            continue;
        }
        // Handle the active screen here
        if (auto mouse = std::get_if<MouseMoveEvent>(&event)) {
            
        }
    }
}

auto Server::handleIncoming(TcpStream socket) -> IoTask<void> {
    ILIAS_CO_TRY(auto endpoint, socket.remoteEndpoint());
    SPDLOG_INFO("Server accepted incoming connection from {}", endpoint);
    struct Guard {
        Server *self;
        IPEndpoint ep;

        ~Guard() {
            SPDLOG_INFO("Server closing connection from {}", ep);
            self->removeScreen(ep);
            self->mClients.erase(ep);
        }
    } guard {this, endpoint};

    ClientState state {
        .transport = RpcTransport {std::move(socket)},
        .sender = {}
    };

    // Ok, begin handshake
    {
        ILIAS_CO_TRY(auto msg, co_await state.transport.readMessage());
        auto hello = std::get_if<HelloMessage>(&msg);
        if (!hello) {
            SPDLOG_ERROR("Server received invalid message");
            co_return Err(RpcError::ProtocolError);
        }
        SPDLOG_INFO("New client connected {}, version: {}, name: {}", endpoint, hello->version, hello->name);
    }

    // Register it
    mClients.emplace(endpoint, &state);

    // Handle the reader part and writer part of the connection
    co_await ilias::whenAny(
        handleClientRead(&state),
        handleClientWrite(&state)
    );
    co_return {};
}

auto Server::handleClientRead(ClientState *state) -> IoTask<void> {
    while (true) {
        ILIAS_CO_TRY(auto msg, co_await state->transport.readMessage());
        // TODO: Handle the message
        SPDLOG_INFO("Server received message {}", msg);
    }
    co_return {};
}

auto Server::handleClientWrite(ClientState *state) -> IoTask<void> {
    using namespace std::literals;
    auto [sender, reader] = ilias::mpsc::channel<RpcMessage>(10);
    state->sender = sender;
    while (true) {
        auto msg = (co_await reader.recv()).value(); // The channel cannot be closed, use .value() to unwrap the value
        ILIAS_CO_TRYV(co_await state->transport.writeMessage(std::move(msg)));
    }
    co_return {};
}

auto Server::addScreen(IPEndpoint endpoint, ScreenInfo info) -> VirtualScreen * {
    auto it = mScreens.emplace(std::pair {endpoint, VirtualScreen {
        .endpoint = endpoint,
        .info = info
    }});
    SPDLOG_INFO("Server current screens {}", mScreens);
    return &it->second;
}

auto Server::removeScreen(IPEndpoint endpoint) -> void {
    mScreens.erase(endpoint);
    SPDLOG_INFO("Server current screens {}", mScreens);
}

// Impl formatter
FORMATTER_IMPL(VirtualScreen);

MKS_END