#pragma once

#include "preinclude.hpp"
#include "core.hpp"
#include "platform/platform.hpp"
#include "rpc/message.hpp"
#include "server_screens.hpp"
#include "server_types.hpp"
#include <ilias/net.hpp>
#include <ilias/sync.hpp>
#include <map>
#include <optional>

MKS_BEGIN

using ilias::IPEndpoint;

/**
 * @brief Routes captured local input through topology to remote clients.
 *
 * Responsibilities:
 * - Track the active @ref VirtualScreen and virtual cursor on remote screens.
 * - Edge-hit and entry-point mapping via @ref ServerScreenStore::topology.
 * - Enqueue @c InputMessage on the peer's mpsc sender (filled by ServerSession).
 * - Drive capture remote-control mode and local cursor warps when returning home.
 *
 * Non-responsibilities:
 * - Accepting sockets or RPC framing (@ref ServerSession).
 * - Persisting layout (@ref ServerScreenStore).
 *
 * Delta policy (v0.1): prefer @c MouseMoveEvent::deltaX/Y; else absolute delta
 * from the last local sample (@c targetDelta = sourceDelta).
 */
class ServerInputRouter {
public:
    /** Endpoint → session write queue for remote InputMessage delivery. */
    using ClientSenders = std::map<IPEndpoint, ilias::mpsc::Sender<RpcMessage>>;

    /**
     * @param screens Topology and VirtualScreen storage (not owned).
     * @param senders Live client writers; missing sender logs and drops the event.
     */
    ServerInputRouter(ServerScreenStore &screens, ClientSenders &senders);

    /**
     * @brief Attach or clear the host InputCapture (nullptr on shutdown).
     */
    auto setCapture(InputCapture *capture) -> void;

    /** @brief Process one captured event (hotkeys, local edge, remote motion). */
    auto handleInputEvent(const InputEvent &event) -> void;

    /** @brief Active screen node inside the store, or null. */
    auto activeScreen() const -> VirtualScreen *;
    auto activeScreenKey() const -> std::optional<ScreenKey>;

    /**
     * @brief Drop active cursor state without choosing a replacement.
     *
     * Call before store nodes that the active pointer referenced are erased.
     */
    auto clearActiveState() -> void;

    /**
     * @brief After screens change, pick a local active screen if needed.
     *
     * When @p preferLocalPrimary is true (local host re-registration), force
     * the local primary/first screen like historical Server behavior.
     */
    auto ensureActiveLocalScreen(bool preferLocalPrimary = false) -> void;

private:
    auto tryHandleLocalHotkey(const InputEvent &event) -> bool;
    auto handleMouseMove(const MouseMoveEvent &event) -> void;
    auto handleRemoteMouseMove(const MouseMoveEvent &event) -> void;
    auto switchActiveScreen(ScreenPoint point) -> void;
    auto eventAtActivePoint(InputEvent event) const -> InputEvent;
    auto suppressPendingLocalWarp(const ScreenPoint &point) -> bool;
    auto moveLocalCursorToActivePoint() -> void;
    auto updateCaptureRemoteControl() -> void;
    auto queueInputForScreen(const VirtualScreen &screen, InputEvent event) -> bool;
    auto activateFirstLocalScreen() -> void;

    ServerScreenStore &mScreens;
    ClientSenders &mSenders;
    InputCapture *mCapture = nullptr;
    // Points into ServerScreenStore::mScreens; invalidate via clearActiveState.
    VirtualScreen *mActiveScreen = nullptr;
    // Pixel position on mActiveScreen. Remote active uses a server-side virtual
    // cursor, not the OS cursor position.
    std::optional<ScreenPoint> mActivePoint;
    // Last absolute local sample for delta fallback when backends omit raw delta.
    std::optional<MouseMoveEvent> mLastLocalMouse;
    // Suppress one local motion echo after SetCursorPos / warp on return home.
    std::optional<ScreenPoint> mPendingLocalWarp;
};

MKS_END
