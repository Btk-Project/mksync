#include "platform/platform.hpp"
#include "rpc/transport.hpp"
#include "rpc/message.hpp"
#include "server.hpp"
#include <algorithm>
#include <ilias/sync.hpp>

MKS_BEGIN

using ilias::TaskScope;

struct ClientState {
    RpcTransport                    transport;
    ilias::mpsc::Sender<RpcMessage> sender; // Use this to send messages to the client
    IPEndpoint                      endpoint;
    std::string                     machineId;
    std::string                     ownerId;
    std::string                     name;
};

Server::Server(IPEndpoint endpoint) : Server(endpoint, AppConfig {}) {

}

Server::Server(IPEndpoint endpoint, AppConfig config)
    : Server(endpoint, std::move(config), {}) {

}

Server::Server(IPEndpoint endpoint, AppConfig config, std::filesystem::path configPath)
    : mEndpoint(endpoint),
      mConfig(std::move(config)),
      mConfigPath(std::move(configPath)) {

}

Server::~Server() {
    
}

auto Server::topologyScreens() const -> std::vector<TopologyScreen> {
    return mTopology.screens();
}

auto Server::activeScreenKey() const -> std::optional<ScreenKey> {
    if (!mActiveScreen) {
        return std::nullopt;
    }
    return mActiveScreen->key;
}

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

auto Server::handleInputEventForTest(const InputEvent &event) -> void {
    handleInputEvent(event);
}

auto Server::isClientTrustedForTest(std::string_view name) const -> bool {
    return isClientTrusted(HelloMessage {
        .version = 0,
        .machineId = {},
        .name = std::string {name},
    });
}

auto Server::isClientTrustedForTest(std::string_view machineId, std::string_view name) const -> bool {
    return isClientTrusted(HelloMessage {
        .version = 0,
        .machineId = std::string {machineId},
        .name = std::string {name},
    });
}

auto Server::configForTest() const -> const AppConfig & {
    return mConfig;
}
#endif

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
    registerScreens(localEndpoint, platform->screens(), true);

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
        handleInputEvent(event);
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
            self->mClientSenders.erase(ep);
            self->mEndpointOwners.erase(ep);
        }
    } guard {this, endpoint};

    ClientState state {
        .transport = RpcTransport {std::move(socket)},
        .sender = {},
        .endpoint = endpoint,
        .machineId = {},
        .ownerId = {},
        .name = {}
    };

    // Ok, begin handshake
    {
        ILIAS_CO_TRY(auto msg, co_await state.transport.readMessage());
        auto hello = std::get_if<HelloMessage>(&msg);
        if (!hello) {
            SPDLOG_ERROR("Server received invalid message");
            co_return Err(RpcError::ProtocolError);
        }
        state.machineId = hello->machineId;
        state.ownerId = state.machineId.empty() ? ownerId(endpoint) : state.machineId;
        mEndpointOwners[endpoint] = state.ownerId;
        state.name = hello->name;
        if (!isClientTrusted(*hello)) {
            SPDLOG_WARN("Server rejected untrusted client {} name={}", endpoint, hello->name);
            co_return Err(RpcError::ProtocolError);
        }
        SPDLOG_INFO("New client connected {}, version: {}, name: {}", endpoint, hello->version, hello->name);
    }

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
        if (auto screens = std::get_if<ScreensMessage>(&msg)) {
            registerScreens(state->endpoint, state->ownerId, screens->screens, false);
            continue;
        }
        SPDLOG_INFO("Server received message {}", msg);
    }
    co_return {};
}

auto Server::isClientTrusted(const HelloMessage &hello) const -> bool {
    return mks::isTrustedClient(mConfig, hello.machineId, hello.name);
}

auto Server::handleClientWrite(ClientState *state) -> IoTask<void> {
    using namespace std::literals;
    auto [sender, reader] = ilias::mpsc::channel<RpcMessage>(10);
    state->sender = sender;
    mClientSenders[state->endpoint] = sender;
    while (true) {
        auto msg = (co_await reader.recv()).value(); // The channel cannot be closed, use .value() to unwrap the value
        ILIAS_CO_TRYV(co_await state->transport.writeMessage(std::move(msg)));
    }
    co_return {};
}

auto Server::handleInputEvent(const InputEvent &event) -> void {
    if (mActiveScreen && !mActiveScreen->local) {
        if (auto mouse = std::get_if<MouseMoveEvent>(&event)) {
            handleRemoteMouseMove(*mouse);
        }
        else {
            queueInputForScreen(*mActiveScreen, event);
        }
        return;
    }

    if (auto key = std::get_if<KeyEvent>(&event)) {
        if (key->key == Key::F12 && !key->release) {
            SPDLOG_INFO("Server F12 pressed");
        }
        return;
    }

    if (auto mouse = std::get_if<MouseMoveEvent>(&event)) {
        handleMouseMove(*mouse);
    }
}

auto Server::handleMouseMove(const MouseMoveEvent &event) -> void {
    if (!mActiveScreen) {
        return;
    }
    if (!mActiveScreen->local) {
        handleRemoteMouseMove(event);
        return;
    }

    auto point = ScreenPoint {
        .key = mActiveScreen->local
            ? ScreenKey {
                .ownerId = mActiveScreen->key.ownerId,
                .screenIndex = event.screenIndex,
            }
            : mActiveScreen->key,
        .x = event.x,
        .y = event.y,
    };
    mLastLocalMouse = event;
    mActivePoint = point;

    auto edge = mTopology.hitEdge(point);
    if (!edge) {
        return;
    }

    auto target = mTopology.mapEntryPoint(point, *edge);
    if (!target) {
        return;
    }

    switchActiveScreen(*target);
}

auto Server::handleRemoteMouseMove(const MouseMoveEvent &event) -> void {
    if (!mActiveScreen || mActiveScreen->local) {
        return;
    }
    if (!mActivePoint || mActivePoint->key != mActiveScreen->key) {
        mActivePoint = ScreenPoint {
            .key = mActiveScreen->key,
            .x = 0,
            .y = 0,
        };
    }

    auto deltaX = 0;
    auto deltaY = 0;
    if (mLastLocalMouse) {
        deltaX = event.x - mLastLocalMouse->x;
        deltaY = event.y - mLastLocalMouse->y;
    }
    mLastLocalMouse = event;

    if (deltaX == 0 && deltaY == 0) {
        return;
    }

    auto nextPoint = *mActivePoint;
    nextPoint.x += deltaX;
    nextPoint.y += deltaY;

    if (auto edge = mTopology.hitEdge(nextPoint)) {
        if (auto target = mTopology.mapEntryPoint(nextPoint, *edge)) {
            switchActiveScreen(*target);
            return;
        }
    }

    const auto maxX = std::max(0, mActiveScreen->info.width - 1);
    const auto maxY = std::max(0, mActiveScreen->info.height - 1);
    mActivePoint->x = std::clamp(nextPoint.x, 0, maxX);
    mActivePoint->y = std::clamp(nextPoint.y, 0, maxY);

    queueInputForScreen(*mActiveScreen, InputEvent {MouseMoveEvent {
        .x = mActivePoint->x,
        .y = mActivePoint->y,
        .screenIndex = mActivePoint->key.screenIndex,
    }});
}

auto Server::switchActiveScreen(ScreenPoint point) -> void {
    auto *screen = findScreen(point.key);
    if (!screen) {
        return;
    }

    if (mActiveScreen && mActiveScreen->key != screen->key) {
        SPDLOG_INFO("Server active screen changed {} -> {}", mActiveScreen->key, screen->key);
    }
    mActiveScreen = screen;
    mActivePoint = std::move(point);

    if (!screen->local) {
        queueInputForScreen(*screen, InputEvent {MouseMoveEvent {
            .x = mActivePoint->x,
            .y = mActivePoint->y,
            .screenIndex = mActivePoint->key.screenIndex,
        }});
    }
}

auto Server::queueInputForScreen(const VirtualScreen &screen, InputEvent event) -> void {
    if (screen.local) {
        return;
    }

    auto it = mClientSenders.find(screen.endpoint);
    if (it == mClientSenders.end() || !it->second) {
        SPDLOG_WARN("Server has no sender for remote screen {}", screen.key);
        return;
    }

    auto message = RpcMessage {InputMessage {
        .event = std::move(event),
    }};
    if (!it->second.trySend(std::move(message))) {
        SPDLOG_WARN("Server failed to queue input for remote screen {}", screen.key);
    }
}

auto Server::registerScreens(IPEndpoint endpoint, const std::vector<ScreenInfo> &screens, bool local) -> void {
    registerScreens(endpoint, defaultOwnerId(endpoint, local), screens, local);
}

auto Server::registerScreens(
    IPEndpoint endpoint,
    std::string_view ownerId,
    const std::vector<ScreenInfo> &screens,
    bool local
) -> void {
    removeScreen(endpoint);

    auto primaryIndex = 0U;
    for (auto index = 0U; index < screens.size(); ++index) {
        if (screens[index].primary) {
            primaryIndex = index;
            break;
        }
    }

    auto primaryRegistered = false;
    for (auto index = 0U; index < screens.size(); ++index) {
        const auto &info = screens[index];
        auto key = ScreenKey {
            .ownerId = std::string {ownerId},
            .screenIndex = index,
        };
        auto cell = GridPosition {};
        if (auto configured = configuredCell(key)) {
            cell = *configured;
            if (local && (info.primary || index == primaryIndex)) {
                primaryRegistered = true;
            }
        }
        else if (local && info.primary && !primaryRegistered) {
            cell = {0, 0};
            primaryRegistered = true;
        }
        else if (local && index == primaryIndex && !primaryRegistered) {
            cell = {0, 0};
            primaryRegistered = true;
        }
        else {
            cell = nextFreeCell(local ? 1 : 1);
        }

        auto *screen = addScreen(
            endpoint,
            std::move(key),
            cell,
            info,
            local
        );
        if (screen && local && (info.primary || (!mActiveScreen && index == 0))) {
            mActiveScreen = screen;
            mActivePoint = ScreenPoint {
                .key = screen->key,
                .x = 0,
                .y = 0,
            };
        }
    }

    SPDLOG_INFO("Server topology screens {}", mTopology.screens());
}

auto Server::addScreen(
    IPEndpoint endpoint,
    ScreenKey key,
    GridPosition cell,
    ScreenInfo info,
    bool local
) -> VirtualScreen * {
    auto topologyResult = mTopology.addScreen(TopologyScreen {
        .key = key,
        .cell = cell,
        .info = info,
        .local = local,
    });
    if (!topologyResult) {
        SPDLOG_ERROR(
            "Server failed to register screen {}:{} in topology: {}",
            key.ownerId,
            key.screenIndex,
            topologyResult.error().message()
        );
        return nullptr;
    }

    auto it = mScreens.emplace(std::pair {endpoint, VirtualScreen {
        .endpoint = endpoint,
        .key = std::move(key),
        .cell = cell,
        .local = local,
        .info = info
    }});
    rememberScreenLayout(it->second.key, it->second.cell);
    SPDLOG_INFO("Server current screens {}", mScreens);
    return &it->second;
}

auto Server::removeScreen(IPEndpoint endpoint) -> void {
    auto range = mScreens.equal_range(endpoint);
    auto ownerIds = std::vector<std::string> {};
    for (auto it = range.first; it != range.second; ++it) {
        if (std::ranges::find(ownerIds, it->second.key.ownerId) == ownerIds.end()) {
            ownerIds.push_back(it->second.key.ownerId);
        }
        if (&it->second == mActiveScreen) {
            mActiveScreen = nullptr;
            mActivePoint.reset();
            mLastLocalMouse.reset();
        }
    }
    mScreens.erase(endpoint);
    for (const auto &registeredOwnerId : ownerIds) {
        mTopology.removeOwner(registeredOwnerId);
    }
    SPDLOG_INFO("Server current screens {}", mScreens);
}

auto Server::findScreen(const ScreenKey &key) -> VirtualScreen * {
    for (auto &[_, screen] : mScreens) {
        if (screen.key == key) {
            return &screen;
        }
    }
    return nullptr;
}

auto Server::findScreen(const ScreenKey &key) const -> const VirtualScreen * {
    for (const auto &[_, screen] : mScreens) {
        if (screen.key == key) {
            return &screen;
        }
    }
    return nullptr;
}

auto Server::configuredCell(const ScreenKey &key) const -> std::optional<GridPosition> {
    return findScreenLayout(mConfig, key.ownerId, key.screenIndex);
}

auto Server::rememberScreenLayout(const ScreenKey &key, GridPosition cell) -> void {
    upsertScreenLayout(mConfig, ScreenLayoutConfig {
        .ownerId = key.ownerId,
        .screenIndex = key.screenIndex,
        .cell = cell,
    });
    saveConfigIfNeeded();
}

auto Server::saveConfigIfNeeded() -> void {
    if (mConfigPath.empty()) {
        return;
    }

    auto saved = saveConfig(mConfigPath, mConfig);
    if (!saved) {
        SPDLOG_WARN("Server failed to save config {}: {}", mConfigPath.string(), saved.error().message());
    }
}

auto Server::nextFreeCell(int32_t startX) const -> GridPosition {
    auto cell = GridPosition {.x = startX, .y = 0};
    while (true) {
        auto occupied = false;
        for (const auto &screen : mTopology.screens()) {
            if (screen.cell == cell) {
                occupied = true;
                break;
            }
        }
        if (!occupied) {
            return cell;
        }
        ++cell.x;
    }
}

auto Server::ownerId(IPEndpoint endpoint) const -> std::string {
    return fmtlib::format("{}", endpoint);
}

auto Server::defaultOwnerId(IPEndpoint endpoint, bool local) const -> std::string {
    if (local && !mConfig.machineId.empty()) {
        return mConfig.machineId;
    }

    if (auto it = mEndpointOwners.find(endpoint); it != mEndpointOwners.end() && !it->second.empty()) {
        return it->second;
    }

    return ownerId(endpoint);
}

// Impl formatter
FORMATTER_IMPL(VirtualScreen);

MKS_END
