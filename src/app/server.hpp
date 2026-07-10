#pragma once

#include "preinclude.hpp"
#include "refl/formatter.hpp"
#include "core.hpp"
#include "config/app_config.hpp"
#include "rpc/message.hpp"
#include <ilias/task.hpp>
#include <ilias/net.hpp>
#include <ilias/sync.hpp>
#include <map>

MKS_BEGIN

using ilias::IPEndpoint;
using ilias::TcpListener;
using ilias::TcpStream;

/**
 * @brief The virtual screen
 * 
 */
struct VirtualScreen {
    // Owner
    IPEndpoint  endpoint;
    ScreenKey   key;
    GridPosition cell;
    bool        local = false;

    // The info
    ScreenInfo  info;
};

inline auto _refl_fmt_inline(const VirtualScreen &value, auto it) {
    return fmtlib::format_to(
        it,
        "VirtualScreen {{ endpoint: {}, key: {}, cell: {}, local: {}, info: {} }}",
        value.endpoint,
        value.key,
        value.cell,
        value.local,
        value.info
    );
}

/**
 * @brief The state of client, manage the outcoming queue
 * 
 */
struct ClientState;
class InputCapture;

/**
 * @brief The server, collection event and dispatch to another client
 * 
 */
class Server {
public:
    explicit Server(IPEndpoint endpoint);
    Server(IPEndpoint endpoint, AppConfig config);
    Server(IPEndpoint endpoint, AppConfig config, std::filesystem::path configPath);
    Server(const Server &) = delete;
    ~Server();

    /**
     * @brief Start the server
     * 
     * @return IoTask<void> 
     */
    auto run() -> IoTask<void>;

    auto topologyScreens() const -> std::vector<TopologyScreen>;
    auto activeScreenKey() const -> std::optional<ScreenKey>;

#ifdef MKS_ENABLE_TEST_HOOKS
    auto registerScreensForTest(
        IPEndpoint endpoint,
        const std::vector<ScreenInfo> &screens,
        bool local
    ) -> void;
    auto registerScreensForTest(
        IPEndpoint endpoint,
        std::string_view ownerId,
        const std::vector<ScreenInfo> &screens,
        bool local
    ) -> void;
    auto attachClientSenderForTest(
        IPEndpoint endpoint,
        ilias::mpsc::Sender<RpcMessage> sender
    ) -> void;
    auto attachCaptureForTest(InputCapture *capture) -> void;
    auto removeScreensForTest(IPEndpoint endpoint) -> void;
    auto handleInputEventForTest(const InputEvent &event) -> void;
    auto isClientTrustedForTest(std::string_view name) const -> bool;
    auto isClientTrustedForTest(std::string_view machineId, std::string_view name) const -> bool;
    auto configForTest() const -> const AppConfig &;
#endif
private:
    // Background tasks
    auto acceptIncomingConnections(TcpListener listener) -> Task<void>;
    auto waitPlatformEvent(void *_platform, void *capture) -> Task<void>;

    // Handle client
    auto handleIncoming(TcpStream stream) -> IoTask<void>;
    auto handleClientRead(ClientState *state) -> IoTask<void>;
    auto handleClientWrite(ClientState *state) -> IoTask<void>;
    auto shutdownClientConnection(ClientState *state) -> Task<void>;
    auto isClientTrusted(const HelloMessage &hello) const -> bool;

    // Input processing
    auto handleInputEvent(const InputEvent &event) -> void;
    auto tryHandleLocalHotkey(const InputEvent &event) -> bool;
    auto handleMouseMove(const MouseMoveEvent &event) -> void;
    auto handleRemoteMouseMove(const MouseMoveEvent &event) -> void;
    auto switchActiveScreen(ScreenPoint point) -> void;
    auto eventAtActivePoint(InputEvent event) const -> InputEvent;
    auto suppressPendingLocalWarp(const ScreenPoint &point) -> bool;
    auto moveLocalCursorToActivePoint() -> void;
    auto updateCaptureRemoteControl() -> void;
    auto queueInputForScreen(const VirtualScreen &screen, InputEvent event) -> bool;

    // Screen Manage
    auto registerScreens(IPEndpoint endpoint, const std::vector<ScreenInfo> &screens, bool local) -> void;
    auto registerScreens(
        IPEndpoint endpoint,
        std::string_view ownerId,
        const std::vector<ScreenInfo> &screens,
        bool local
    ) -> void;
    auto addScreen(
        IPEndpoint endpoint,
        ScreenKey key,
        GridPosition cell,
        ScreenInfo info,
        bool local
    ) -> VirtualScreen *;
    auto removeScreen(IPEndpoint endpoint) -> void;
    auto activateFirstLocalScreen() -> void;
    auto findScreen(const ScreenKey &key) -> VirtualScreen *;
    auto findScreen(const ScreenKey &key) const -> const VirtualScreen *;
    auto configuredCell(const ScreenKey &key) const -> std::optional<GridPosition>;
    auto rememberScreenLayout(const ScreenKey &key, GridPosition cell) -> void;
    auto saveConfigIfNeeded() -> void;
    auto nextFreeCell(int32_t startX) const -> GridPosition;
    auto ownerId(IPEndpoint endpoint) const -> std::string;
    auto defaultOwnerId(IPEndpoint endpoint, bool local) const -> std::string;

    IPEndpoint  mEndpoint;
    TcpListener mListener;
    AppConfig   mConfig;
    std::filesystem::path mConfigPath;

    // Client connections
    std::multimap<IPEndpoint, VirtualScreen> mScreens; // Mapping (owner) to some virtual screen
    ScreenTopology                            mTopology;
    std::map<IPEndpoint, ilias::mpsc::Sender<RpcMessage>> mClientSenders;
    std::map<IPEndpoint, std::string> mEndpointOwners;

    // Current active screen that owns keyboard/mouse input.
    VirtualScreen *mActiveScreen = nullptr;
    InputCapture *mCapture = nullptr;

    // Pixel position on mActiveScreen. When the active screen is remote this is
    // a virtual cursor owned by the server, not the local OS cursor position.
    std::optional<ScreenPoint> mActivePoint;

    // Last absolute local capture sample. Remote motion currently uses
    // targetDelta = sourceDelta, so this sample is the source delta baseline.
    std::optional<MouseMoveEvent> mLastLocalMouse;

    // XWarpPointer/SetCursorPos may echo one local motion event at the mapped
    // edge pixel. Suppress that echo so returning home does not bounce back.
    std::optional<ScreenPoint> mPendingLocalWarp;
};

MKS_END
