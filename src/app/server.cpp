#include "server.hpp"
#include "server_session.hpp"

#include <cassert>
#include <ilias/sync.hpp>
#include <utility>

MKS_BEGIN

using ilias::TaskScope;

// MARK: Construction

Server::Server(Platform::Ptr platform, IPEndpoint endpoint)
    : Server(std::move(platform), endpoint, AppConfig {}) {
}

Server::Server(Platform::Ptr platform, IPEndpoint endpoint, AppConfig config)
    : Server(std::move(platform), endpoint, std::move(config), {}) {
}

Server::Server(
    Platform::Ptr platform,
    IPEndpoint endpoint,
    AppConfig config,
    std::filesystem::path configPath
)
    : mPlatform(std::move(platform)),
      mEndpoint(endpoint),
      mScreens(std::move(config), std::move(configPath)),
      mClientSenders(),
      mInput(mScreens, mClientSenders) {
    // Interface invariant: callers inject a live Platform (MockPlatform in
    // tests, Platform::create() in main). Null is a programming error.
    assert(mPlatform);
}

Server::~Server() = default;

// MARK: Public queries

auto Server::topologyScreens() const -> std::vector<TopologyScreen> {
    return mScreens.topologyScreens();
}

auto Server::activeScreenKey() const -> std::optional<ScreenKey> {
    return mInput.activeScreenKey();
}

// MARK: Test hooks

#ifdef MKS_ENABLE_TEST_HOOKS
auto Server::registerScreensForTest(
    IPEndpoint endpoint,
    const std::vector<ScreenInfo> &screens,
    bool local
) -> void {
    registerScreens(endpoint, screens, local);
}

auto Server::registerScreensForTest(
    IPEndpoint endpoint,
    std::string_view ownerId,
    const std::vector<ScreenInfo> &screens,
    bool local
) -> void {
    registerScreens(endpoint, ownerId, screens, local);
}

auto Server::attachClientSenderForTest(
    IPEndpoint endpoint,
    ilias::mpsc::Sender<RpcMessage> sender
) -> void {
    mClientSenders[endpoint] = std::move(sender);
}

auto Server::attachCaptureForTest(InputCapture *capture) -> void {
    mInput.setCapture(capture);
}

auto Server::removeScreensForTest(IPEndpoint endpoint) -> void {
    removeEndpointScreens(endpoint);
}

auto Server::handleInputEventForTest(const InputEvent &event) -> void {
    mInput.handleInputEvent(event);
}

auto Server::isClientTrustedForTest(std::string_view name) const -> bool {
    return mks::isTrustedClient(mScreens.config(), {}, name);
}

auto Server::isClientTrustedForTest(
    std::string_view machineId,
    std::string_view name
) const -> bool {
    return mks::isTrustedClient(mScreens.config(), machineId, name);
}

auto Server::configForTest() const -> const AppConfig & {
    return mScreens.config();
}
#endif

// MARK: Run

auto Server::run() -> IoTask<void> {
    ILIAS_CO_TRY(auto listener, co_await TcpListener::bind(mEndpoint));
    ILIAS_CO_TRY(auto localEndpoint, listener.localEndpoint());
    SPDLOG_INFO("Server listening on {}", localEndpoint);

    auto capture = mPlatform->createCapture();
    if (!capture) {
        SPDLOG_ERROR("Current platform does not provide an input capture backend");
        co_return Err(std::make_error_code(std::errc::operation_not_supported));
    }
    ILIAS_CO_TRYV(co_await capture->initialize());
    mInput.setCapture(capture.get());

    // Local screens anchor the topology at (0,0) primary / free cells to the right.
    registerScreens(localEndpoint, mPlatform->screens(), true);

    co_await ilias::finally(
        ilias::whenAll(
            acceptIncomingConnections(std::move(listener)),
            waitPlatformEvent(*capture)
        ),
        capture->shutdown()
    );
    mInput.setCapture(nullptr);
    co_return {};
}

auto Server::acceptIncomingConnections(TcpListener listener) -> Task<void> {
    // TaskScope cancels remaining sessions when this accept task is cancelled
    // (e.g. run() finally / process shutdown).
    co_return co_await TaskScope::enter([&](auto &scope) -> Task<void> {
        while (true) {
            auto incoming = co_await listener.accept();
            if (!incoming) {
                SPDLOG_ERROR("Server failed to accept incoming connection");
                co_return;
            }
            auto &[sock, endpoint] = *incoming;
            (void) endpoint;
            scope.spawn(handleIncoming(std::move(sock)));
        }
    });
}

auto Server::waitPlatformEvent(InputCapture &capture) -> Task<void> {
    SPDLOG_INFO("Server waiting for platform events");
    while (true) {
        auto event = co_await capture.nextEvent();
        SPDLOG_TRACE("Server captured platform event {}", event);
        mInput.handleInputEvent(event);
    }
}

auto Server::handleIncoming(TcpStream stream) -> IoTask<void> {
    ILIAS_CO_TRY(auto endpoint, stream.remoteEndpoint());

    // Session borrows host state; onClosed / onScreens keep active-screen
    // pointers consistent when map nodes are erased.
    auto session = ServerSession {
        ServerSession::Context {
            .screens = mScreens,
            .senders = mClientSenders,
            .onScreens = [this](
                IPEndpoint ep,
                std::string_view ownerId,
                const std::vector<ScreenInfo> &screens
            ) {
                registerScreens(ep, ownerId, screens, false);
            },
            .onClosed = [this](IPEndpoint ep) {
                removeEndpointScreens(ep);
                mClientSenders.erase(ep);
                mScreens.forgetOwner(ep);
            },
        },
        std::move(stream),
        endpoint,
    };
    co_return co_await session.run();
}

// MARK: Screen coordination

auto Server::registerScreens(
    IPEndpoint endpoint,
    const std::vector<ScreenInfo> &screens,
    bool local
) -> void {
    registerScreens(endpoint, mScreens.defaultOwnerId(endpoint, local), screens, local);
}

auto Server::registerScreens(
    IPEndpoint endpoint,
    std::string_view ownerId,
    const std::vector<ScreenInfo> &screens,
    bool local
) -> void {
    // Full replacement for the endpoint: drop old cells, then add the new set.
    // Active VirtualScreen* lives in the router and points into the store, so
    // clear it before removeScreen erases multimap nodes.
    if (mScreens.removeScreen(endpoint, mInput.activeScreen())) {
        mInput.clearActiveState();
    }
    mScreens.registerScreens(endpoint, ownerId, screens, local);
    mInput.ensureActiveLocalScreen(local);
}

auto Server::removeEndpointScreens(IPEndpoint endpoint) -> void {
    if (mScreens.removeScreen(endpoint, mInput.activeScreen())) {
        mInput.clearActiveState();
        mInput.ensureActiveLocalScreen();
    }
}

// Impl formatter for VirtualScreen (declared via _refl_fmt_inline in types).
FORMATTER_IMPL(VirtualScreen);

MKS_END
