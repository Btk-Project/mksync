#include "server_input.hpp"

#include <algorithm>
#include <utility>

MKS_BEGIN

// MARK: Lifecycle / active screen

ServerInputRouter::ServerInputRouter(ServerScreenStore &screens, ClientSenders &senders)
    : mScreens(screens),
      mSenders(senders) {
}

auto ServerInputRouter::setCapture(InputCapture *capture) -> void {
    mCapture = capture;
    updateCaptureRemoteControl();
}

auto ServerInputRouter::activeScreen() const -> VirtualScreen * {
    return mActiveScreen;
}

auto ServerInputRouter::activeScreenKey() const -> std::optional<ScreenKey> {
    if (!mActiveScreen) {
        return std::nullopt;
    }
    return mActiveScreen->key;
}

auto ServerInputRouter::clearActiveState() -> void {
    mActiveScreen = nullptr;
    mActivePoint.reset();
    mLastLocalMouse.reset();
    mPendingLocalWarp.reset();
    updateCaptureRemoteControl();
}

auto ServerInputRouter::ensureActiveLocalScreen(bool preferLocalPrimary) -> void {
    if (preferLocalPrimary) {
        // Initial server startup only establishes which local screen is active.
        // There is no remote cursor to bring home, so do not reuse the
        // remote-to-local switch path: that path intentionally warps the OS
        // pointer to the mapped entry point.
        if (!mActiveScreen) {
            activateFirstLocalScreen();
            return;
        }
        if (auto *local = mScreens.firstLocalScreen()) {
            switchActiveScreen(ScreenPoint {
                .key = local->key,
                .x = 0,
                .y = 0,
            });
            return;
        }
    }
}

// MARK: Event entry

auto ServerInputRouter::handleInputEvent(const InputEvent &event) -> void {
    SPDLOG_TRACE(
        "Server handling input event active={} point={} event={}",
        mActiveScreen ? fmtlib::format("{}", mActiveScreen->key) : std::string {"<none>"},
        mActivePoint ? fmtlib::format("{}", *mActivePoint) : std::string {"<none>"},
        event
    );

    if (tryHandleLocalHotkey(event)) {
        SPDLOG_TRACE("Server consumed local hotkey event {}", event);
        return;
    }

    if (mActiveScreen && !mActiveScreen->local) {
        // While a remote screen is active, all non-mouse input belongs to that
        // client. Mouse movement still needs special handling because the local
        // capture backend reports absolute local pixels, not remote pixels.
        if (auto mouse = std::get_if<MouseMoveEvent>(&event)) {
            handleRemoteMouseMove(*mouse);
        }
        else {
            auto routed = eventAtActivePoint(event);
            SPDLOG_TRACE(
                "Server routing non-move event to remote screen {} source={} routed={}",
                mActiveScreen->key,
                event,
                routed
            );
            queueInputForScreen(*mActiveScreen, std::move(routed));
        }
        return;
    }

    if (auto key = std::get_if<KeyEvent>(&event)) {
        (void) key;
        return;
    }

    if (auto mouse = std::get_if<MouseMoveEvent>(&event)) {
        handleMouseMove(*mouse);
    }
}

// MARK: Hotkeys / mouse

auto ServerInputRouter::tryHandleLocalHotkey(const InputEvent &event) -> bool {
    auto key = std::get_if<KeyEvent>(&event);
    if (!key || key->release || key->key != Key::F12) {
        return false;
    }

    if (mActiveScreen && !mActiveScreen->local) {
        SPDLOG_INFO("Server fail-safe F12 pressed, returning to local screen");
        activateFirstLocalScreen();
        return true;
    }

    SPDLOG_INFO("Server F12 pressed");
    return true;
}

auto ServerInputRouter::handleMouseMove(const MouseMoveEvent &event) -> void {
    if (!mActiveScreen) {
        SPDLOG_TRACE("Server ignored local mouse move because no active screen exists: {}", event);
        return;
    }
    if (!mActiveScreen->local) {
        handleRemoteMouseMove(event);
        return;
    }

    // Local capture events carry a screenIndex from the local platform. Rebuild
    // the key so multi-monitor local hosts can cross from any local screen.
    auto point = ScreenPoint {
        .key = ScreenKey {
            .ownerId = mActiveScreen->key.ownerId,
            .screenIndex = event.screenIndex,
        },
        .x = event.x,
        .y = event.y,
    };
    // Keep the latest local sample even if we do not cross yet. If the next
    // event is handled as remote motion, this becomes the delta baseline.
    mLastLocalMouse = event;
    mActivePoint = point;
    SPDLOG_TRACE("Server local mouse point {}", point);

    if (suppressPendingLocalWarp(point)) {
        return;
    }

    auto edge = mScreens.topology().hitEdge(point);
    if (!edge) {
        SPDLOG_TRACE("Server local mouse remains on {}", point.key);
        return;
    }
    SPDLOG_TRACE("Server local mouse hit edge {} at {}", *edge, point);

    auto target = mScreens.topology().mapEntryPoint(point, *edge);
    if (!target) {
        SPDLOG_TRACE(
            "Server local mouse edge {} has no mapped target from {}: {}",
            *edge,
            point,
            target.error().message()
        );
        return;
    }
    SPDLOG_TRACE("Server local mouse maps {} across {} to {}", point, *edge, *target);

    switchActiveScreen(*target);
}

auto ServerInputRouter::handleRemoteMouseMove(const MouseMoveEvent &event) -> void {
    if (!mActiveScreen || mActiveScreen->local) {
        SPDLOG_TRACE("Server ignored remote mouse move without remote active screen: {}", event);
        return;
    }
    // A remote active screen has no local OS cursor to query. mActivePoint is
    // the server-side virtual cursor in the remote screen's real pixel space.
    if (!mActivePoint || mActivePoint->key != mActiveScreen->key) {
        mActivePoint = ScreenPoint {
            .key = mActiveScreen->key,
            .x = 0,
            .y = 0,
        };
    }

    // Prefer raw relative motion when the backend provides it. Absolute local
    // coordinates stop changing at the physical edge, but raw deltas keep the
    // remote virtual cursor moving beyond that boundary.
    auto deltaX = event.deltaX;
    auto deltaY = event.deltaY;
    if (deltaX == 0 && deltaY == 0 && mLastLocalMouse) {
        deltaX = event.x - mLastLocalMouse->x;
        deltaY = event.y - mLastLocalMouse->y;
    }
    mLastLocalMouse = event;
    SPDLOG_TRACE(
        "Server remote mouse source={} delta=({}, {}) activePoint={}",
        event,
        deltaX,
        deltaY,
        *mActivePoint
    );

    if (deltaX == 0 && deltaY == 0) {
        SPDLOG_TRACE("Server ignored zero-delta remote mouse event {}", event);
        return;
    }

    auto nextPoint = *mActivePoint;
    nextPoint.x += deltaX;
    nextPoint.y += deltaY;
    SPDLOG_TRACE("Server remote virtual cursor candidate {}", nextPoint);

    // Check crossing before clamping so an overshoot past the remote edge can
    // move into the neighbor instead of getting stuck at the border pixel.
    if (auto edge = mScreens.topology().hitEdge(nextPoint)) {
        SPDLOG_TRACE("Server remote virtual cursor hit edge {} at {}", *edge, nextPoint);
        if (auto target = mScreens.topology().mapEntryPoint(nextPoint, *edge)) {
            SPDLOG_TRACE("Server remote mouse maps {} across {} to {}", nextPoint, *edge, *target);
            switchActiveScreen(*target);
            return;
        }
        else {
            SPDLOG_TRACE(
                "Server remote edge {} has no mapped target from {}: {}",
                *edge,
                nextPoint,
                target.error().message()
            );
        }
    }

    const auto maxX = std::max(0, mActiveScreen->info.width - 1);
    const auto maxY = std::max(0, mActiveScreen->info.height - 1);
    // No neighbor accepted the movement, so keep the virtual cursor inside the
    // active remote screen and send an absolute pixel position to the client.
    mActivePoint->x = std::clamp(nextPoint.x, 0, maxX);
    mActivePoint->y = std::clamp(nextPoint.y, 0, maxY);
    SPDLOG_TRACE("Server remote virtual cursor clamped to {}", *mActivePoint);

    queueInputForScreen(*mActiveScreen, InputEvent {MouseMoveEvent {
        .x = mActivePoint->x,
        .y = mActivePoint->y,
        .screenIndex = mActivePoint->key.screenIndex,
    }});
}

// MARK: Screen switch / capture

auto ServerInputRouter::switchActiveScreen(ScreenPoint point) -> void {
    auto *screen = mScreens.findScreen(point.key);
    if (!screen) {
        return;
    }

    if (mActiveScreen && mActiveScreen->key != screen->key) {
        SPDLOG_INFO(
            "Server active screen changed {} -> {} at {}",
            mActiveScreen->key,
            screen->key,
            point
        );
    }
    else {
        SPDLOG_TRACE("Server active screen stays on {} at {}", screen->key, point);
    }
    mActiveScreen = screen;
    mActivePoint = std::move(point);
    updateCaptureRemoteControl();

    if (!screen->local) {
        mPendingLocalWarp.reset();
        // Entering a remote screen needs an immediate absolute move so the
        // client-side injector starts from the mapped entry pixel.
        auto entry = MouseMoveEvent {
            .x = mActivePoint->x,
            .y = mActivePoint->y,
            .screenIndex = mActivePoint->key.screenIndex,
        };
        if (queueInputForScreen(*screen, InputEvent {entry})) {
            SPDLOG_INFO(
                "Server queued remote cursor entry endpoint={} screen={} event={}",
                screen->endpoint,
                screen->key,
                entry
            );
        }
    }
    else {
        moveLocalCursorToActivePoint();
    }
}

auto ServerInputRouter::eventAtActivePoint(InputEvent event) const -> InputEvent {
    if (!mActiveScreen || !mActivePoint || mActivePoint->key != mActiveScreen->key) {
        return event;
    }

    std::visit(Overloads {
        [&](MouseButtonEvent &button) {
            button.x = mActivePoint->x;
            button.y = mActivePoint->y;
            button.screenIndex = mActivePoint->key.screenIndex;
        },
        [&](MouseWheelEvent &wheel) {
            wheel.x = mActivePoint->x;
            wheel.y = mActivePoint->y;
        },
        [](MouseMoveEvent &) {},
        [](KeyEvent &) {},
    }, event);
    return event;
}

auto ServerInputRouter::suppressPendingLocalWarp(const ScreenPoint &point) -> bool {
    if (!mPendingLocalWarp || *mPendingLocalWarp != point) {
        return false;
    }

    SPDLOG_TRACE("Server suppressed local warp echo at {}", point);
    mPendingLocalWarp.reset();
    return true;
}

auto ServerInputRouter::moveLocalCursorToActivePoint() -> void {
    if (!mCapture || !mActiveScreen || !mActiveScreen->local || !mActivePoint) {
        return;
    }

    auto result = mCapture->moveLocalCursor(
        mActivePoint->key.screenIndex,
        mActivePoint->x,
        mActivePoint->y
    );
    if (!result) {
        SPDLOG_WARN(
            "Server failed to move local cursor to {}: {}",
            *mActivePoint,
            result.error().message()
        );
        return;
    }

    mPendingLocalWarp = *mActivePoint;
    SPDLOG_TRACE("Server moved local cursor to {}", *mActivePoint);
}

auto ServerInputRouter::updateCaptureRemoteControl() -> void {
    if (!mCapture) {
        return;
    }

    const auto active = mActiveScreen && !mActiveScreen->local;
    auto result = mCapture->setRemoteControlActive(active);
    if (!result) {
        SPDLOG_WARN(
            "Server failed to {} local input capture for remote control: {}",
            active ? "enable" : "disable",
            result.error().message()
        );
    }
}

auto ServerInputRouter::queueInputForScreen(const VirtualScreen &screen, InputEvent event) -> bool {
    if (screen.local) {
        SPDLOG_TRACE("Server ignored queue for local screen {} event={}", screen.key, event);
        return false;
    }

    auto it = mSenders.find(screen.endpoint);
    if (it == mSenders.end() || !it->second) {
        SPDLOG_WARN("Server has no sender for remote screen {}", screen.key);
        return false;
    }

    auto message = RpcMessage {InputMessage {
        .event = std::move(event),
    }};
    SPDLOG_TRACE(
        "Server queueing input for remote screen {} endpoint={} message={}",
        screen.key,
        screen.endpoint,
        message
    );
    if (!it->second.trySend(std::move(message))) {
        SPDLOG_WARN("Server failed to queue input for remote screen {}", screen.key);
        return false;
    }
    return true;
}

auto ServerInputRouter::activateFirstLocalScreen() -> void {
    auto *screen = mScreens.firstLocalScreen();
    if (!screen) {
        mActiveScreen = nullptr;
        mActivePoint.reset();
        mLastLocalMouse.reset();
        mPendingLocalWarp.reset();
        updateCaptureRemoteControl();
        return;
    }

    mActiveScreen = screen;
    mActivePoint = ScreenPoint {
        .key = mActiveScreen->key,
        .x = 0,
        .y = 0,
    };
    mLastLocalMouse.reset();
    mPendingLocalWarp.reset();
    updateCaptureRemoteControl();
    SPDLOG_INFO("Server active screen reset to {}", mActiveScreen->key);
}

MKS_END
