#include "host.hpp"

#include <algorithm>
#include <cmath>
#include <span>
#include <vector>

#include <spdlog/spdlog.h>

namespace mksync::app {

using domain::Edge;
using domain::FocusTarget;
using domain::RouteDecision;
using domain::ScreenBounds;
using domain::ScreenId;
using platform::InputEvent;

namespace {
auto edgeName(Edge edge) -> const char * {
    switch (edge) {
        case Edge::Left: return "Left";
        case Edge::Right: return "Right";
        case Edge::Top: return "Top";
        case Edge::Bottom: return "Bottom";
    }
    return "Unknown";
}

auto actionName(RouteDecision::Action action) -> const char * {
    using enum RouteDecision::Action;
    switch (action) {
        case Noop: return "Noop";
        case KeepLocal: return "KeepLocal";
        case ForwardToRemote: return "ForwardToRemote";
        case SwitchToRemote: return "SwitchToRemote";
        case ReturnLocal: return "ReturnLocal";
    }
    return "Unknown";
}

auto frameTypeForEventType(InputEvent::Type type) -> proto::MessageType {
    using enum InputEvent::Type;
    switch (type) {
        case MouseMove: return proto::MessageType::MouseMove;
        case MousePress: return proto::MessageType::MousePress;
        case MouseRelease: return proto::MessageType::MouseRelease;
        case MouseWheel: return proto::MessageType::MouseWheel;
        case KeyPress: return proto::MessageType::KeyPress;
        case KeyRelease: return proto::MessageType::KeyRelease;
        default: return proto::MessageType::Error;
    }
}

auto orderedScreenIds(std::span<const platform::ScreenInfo> screens, std::string_view nodeName) -> std::vector<ScreenId> {
    struct OrderedScreen {
        uint32_t index;
        ScreenBounds bounds;
    };

    std::vector<OrderedScreen> ordered;
    ordered.reserve(screens.size());
    for (size_t i = 0; i < screens.size(); ++i) {
        ordered.push_back(OrderedScreen {
            .index = static_cast<uint32_t>(i),
            .bounds = ScreenBounds {.x = screens[i].x, .y = screens[i].y, .width = screens[i].width, .height = screens[i].height},
        });
    }

    std::sort(ordered.begin(), ordered.end(), [](const auto &lhs, const auto &rhs) {
        return lhs.bounds.x == rhs.bounds.x ? lhs.bounds.y < rhs.bounds.y : lhs.bounds.x < rhs.bounds.x;
    });

    std::vector<ScreenId> result;
    result.reserve(ordered.size());
    for (const auto &screen : ordered) {
        result.push_back(ScreenId {std::string(nodeName), screen.index});
    }
    return result;
}

auto makeScreenInfoFrame(uint32_t index, const platform::ScreenInfo &screen) -> proto::Frame {
    return proto::Frame {
        .type = proto::MessageType::ScreenInfo,
        .payload = proto::ScreenInfo {
            .index = index,
            .x = screen.x,
            .y = screen.y,
            .width = screen.width,
            .height = screen.height,
            .dpi = screen.dpi,
            .name = screen.name,
            .primary = screen.primary,
        },
    };
}

auto clampCoord(int32_t value, int32_t extent) -> int32_t {
    if (extent <= 0) {
        return 0;
    }
    return std::clamp(value, 0, extent - 1);
}

auto mapAxis(int32_t value, int32_t fromExtent, int32_t toExtent) -> int32_t {
    if (fromExtent <= 1 || toExtent <= 1) {
        return 0;
    }
    auto ratio = static_cast<double>(std::clamp(value, 0, fromExtent - 1)) / static_cast<double>(fromExtent - 1);
    return static_cast<int32_t>(std::lround(ratio * static_cast<double>(toExtent - 1)));
}
} // namespace

auto HostApp::run(const ilias::IPEndpoint &bind) -> ilias::IoTask<void> {
    mBind = bind;
    if (auto init = co_await initialize(); !init) {
        co_return init;
    }

    auto [eventResult, listenerResult] = co_await ilias::finally(
        ilias::whenAll(eventLoop(), listenerLoop()),
        [this]() -> ilias::Task<void> {
            co_await shutdown();
        }
    );
    if (!eventResult) {
        co_return ilias::Err(eventResult.error());
    }
    if (!listenerResult) {
        co_return ilias::Err(listenerResult.error());
    }
    co_return {};
}

auto HostApp::initialize() -> ilias::IoTask<void> {
    mPlatform = platform::createPlatform();
    if (!mPlatform) {
        co_return ilias::Err(make_error_code(std::errc::not_supported));
    }

    mCapture = mPlatform->createInputCapture();
    mInjector = mPlatform->createInputInjector();
    if (!mCapture) {
        co_return ilias::Err(make_error_code(std::errc::not_supported));
    }

    if (auto res = co_await mCapture->initialize(); !res) {
        co_return res;
    }

    if (mInjector) {
        if (auto res = co_await mInjector->initialize(); !res) {
            SPDLOG_WARN("Input injector initialization failed: {}", res.error().message());
            mInjector.reset();
        }
    }

    mTopology.setLocalNode("local");
    reloadLocalTopology();
    mFocus.setTopology(&mTopology);
    mFocus.setEdgeThreshold(0);
    if (auto screen = mTopology.localScreen(0); screen) {
        mFocus.activateLocal(*screen);
    }

    SPDLOG_INFO("HostApp initialized with {} local screen(s), bind={}", mPlatform->screens().size(), mBind);
    co_return {};
}

auto HostApp::shutdown() -> ilias::Task<void> {
    mActiveSender = {};
    clearRemoteTopology();
    if (mCapture) {
        co_await mCapture->shutdown();
    }
    if (mInjector) {
        co_await mInjector->shutdown();
    }
    SPDLOG_INFO("HostApp shutdown complete");
    co_return;
}

auto HostApp::eventLoop() -> ilias::IoTask<void> {
    while (true) {
        auto event = co_await mCapture->nextEvent();
        if (!event) {
            co_return ilias::Err(event.error());
        }
        handleEvent(*event);
    }
}

auto HostApp::listenerLoop() -> ilias::IoTask<void> {
    auto listener = co_await ilias::TcpListener::bind(mBind);
    if (!listener) {
        co_return ilias::Err(listener.error());
    }

    SPDLOG_INFO("Host listener bound at {}", listener->localEndpoint().value());
    while (true) {
        auto conn = co_await listener->accept();
        if (!conn) {
            SPDLOG_WARN("Accept failed: {}", conn.error().message());
            continue;
        }

        auto &[stream, endpoint] = *conn;
        SPDLOG_INFO("Accepted client from {}", endpoint);
        auto result = co_await handleSession(std::move(stream));
        if (!result) {
            SPDLOG_WARN("Client session ended: {}", result.error().message());
        }
    }
}

auto HostApp::handleSession(ilias::TcpStream tcp) -> ilias::IoTask<void> {
    transport::BufferedTcpStream stream {std::move(tcp)};

    auto helloFrame = co_await transport::readFrame(stream);
    if (!helloFrame) {
        co_return ilias::Err(helloFrame.error());
    }
    if (helloFrame->type != proto::MessageType::Hello) {
        co_return ilias::Err(make_error_code(std::errc::protocol_error));
    }

    auto hello = std::get_if<proto::Hello>(&helloFrame->payload);
    if (hello == nullptr) {
        co_return ilias::Err(make_error_code(std::errc::protocol_error));
    }

    SPDLOG_INFO(
        "Client hello: version={}, device='{}', screens={}",
        hello->version,
        hello->deviceName,
        hello->screenCount
    );

    auto localScreens = mPlatform->screens();
    if (auto ack = co_await transport::writeFrame(stream, transport::makeHelloAck(static_cast<uint32_t>(localScreens.size()))); !ack) {
        co_return ack;
    }
    SPDLOG_INFO("Host sent HelloAck to '{}'", hello->deviceName);

    for (uint32_t i = 0; i < localScreens.size(); ++i) {
        if (auto written = co_await transport::writeFrame(stream, makeScreenInfoFrame(i, localScreens[i])); !written) {
            co_return written;
        }
    }
    SPDLOG_INFO("Host announced {} local screen(s)", localScreens.size());

    std::vector<platform::ScreenInfo> remoteScreens;
    remoteScreens.reserve(hello->screenCount);
    for (uint32_t i = 0; i < hello->screenCount; ++i) {
        auto frame = co_await transport::readFrame(stream);
        if (!frame) {
            co_return ilias::Err(frame.error());
        }
        if (frame->type != proto::MessageType::ScreenInfo) {
            co_return ilias::Err(make_error_code(std::errc::protocol_error));
        }

        auto screen = std::get_if<proto::ScreenInfo>(&frame->payload);
        if (screen == nullptr) {
            co_return ilias::Err(make_error_code(std::errc::protocol_error));
        }

        remoteScreens.push_back(platform::ScreenInfo {
            .x = screen->x,
            .y = screen->y,
            .width = screen->width,
            .height = screen->height,
            .dpi = screen->dpi,
            .name = screen->name,
            .primary = screen->primary,
        });
    }

    updateRemoteTopology(std::format("remote:{}", hello->deviceName), remoteScreens);
    SPDLOG_INFO("Remote topology updated for '{}' with {} screen(s)", hello->deviceName, remoteScreens.size());

    auto pair = ilias::mpsc::channel<proto::Frame>(512);
    mActiveSender = pair.sender;

    auto [readResult, writeResult] = co_await ilias::finally(
        ilias::whenAny(
            sessionReadLoop(stream, pair.sender),
            sessionWriteLoop(stream, std::move(pair.receiver))
        ),
        [this]() -> ilias::Task<void> {
            mActiveSender = {};
            clearRemoteTopology();
            co_return;
        }
    );

    if (readResult && !*readResult) {
        co_return ilias::Err(readResult->error());
    }
    if (writeResult && !*writeResult) {
        co_return ilias::Err(writeResult->error());
    }
    co_return {};
}

auto HostApp::sessionReadLoop(transport::BufferedTcpStream &stream, ilias::mpsc::Sender<proto::Frame> sender) -> ilias::IoTask<void> {
    while (true) {
        auto frame = co_await transport::readFrame(stream);
        if (!frame) {
            co_return ilias::Err(frame.error());
        }

        switch (frame->type) {
            case proto::MessageType::Ping:
                if (auto ping = std::get_if<proto::Ping>(&frame->payload)) {
                    if (auto queued = sender.trySend(transport::makePong(ping->timestampUs)); !queued) {
                        co_return ilias::Err(make_error_code(std::errc::operation_canceled));
                    }
                    SPDLOG_DEBUG("Queued Pong for client timestampUs={}", ping->timestampUs);
                }
                break;
            case proto::MessageType::Pong:
                SPDLOG_DEBUG("Host received Pong");
                break;
            case proto::MessageType::FocusLeave:
                if (auto focus = std::get_if<proto::FocusLeave>(&frame->payload)) {
                    auto target = currentRemoteTarget();
                    if (target && target->screen.index == focus->screenIndex) {
                        if (auto local = mTopology.localScreen(mRemoteCursor ? mRemoteCursor->localScreenIndex : 0); local) {
                            mFocus.activateLocal(*local);
                        }
                        endRemotePointerCapture(true);
                        SPDLOG_INFO("Host returned focus to local from remote screen {} on client request", focus->screenIndex);
                    }
                }
                break;
            default:
                SPDLOG_DEBUG("Host ignored inbound frame type {}", frame->type);
                break;
        }
    }
}

auto HostApp::sessionWriteLoop(transport::BufferedTcpStream &stream, ilias::mpsc::Receiver<proto::Frame> receiver) -> ilias::IoTask<void> {
    while (true) {
        auto frame = co_await receiver.recv();
        if (!frame) {
            co_return ilias::Err(make_error_code(std::errc::operation_canceled));
        }
        SPDLOG_DEBUG("Host sending frame {}", frame->type);
        if (auto written = co_await transport::writeFrame(stream, *frame); !written) {
            co_return written;
        }
    }
}

auto HostApp::handleEvent(const InputEvent &event) -> void {
    if (event.metadata.injected) {
        SPDLOG_DEBUG("Ignoring injected event: {}", event);
        return;
    }

    if (event.type == InputEvent::Type::ScreenChange) {
        reloadLocalTopology();
        SPDLOG_INFO("Topology reloaded after screen change; local screens={}", mPlatform->screens().size());
        return;
    }

    if (interceptLocalHotkey(event)) {
        return;
    }

    auto forwardToRemote = false;
    auto transitionEdge = std::optional<Edge> {};
    auto focusEntered = false;

    if (event.type == InputEvent::Type::MouseMove) {
        auto data = event.getIf<InputEvent::MouseMoveData>();
        if (data == nullptr) {
            return;
        }

        if (handleRemotePointerMotion(event, *data)) {
            return;
        }

        auto screen = mTopology.localScreen(event.metadata.screenIndex);
        if (!screen) {
            SPDLOG_DEBUG("MouseMove without known local screen: {}", event);
            return;
        }

        auto decision = mFocus.handlePointer(*screen, data->x, data->y);
        switch (decision.action) {
            case RouteDecision::Action::SwitchToRemote:
                forwardToRemote = true;
                transitionEdge = decision.edge;
                focusEntered = true;
                break;
            case RouteDecision::Action::ForwardToRemote:
                forwardToRemote = true;
                break;
            case RouteDecision::Action::ReturnLocal:
            case RouteDecision::Action::KeepLocal:
            case RouteDecision::Action::Noop:
                forwardToRemote = false;
                break;
        }

        if (decision.action == RouteDecision::Action::SwitchToRemote || decision.action == RouteDecision::Action::ReturnLocal) {
            if (decision.target) {
                SPDLOG_INFO(
                    "Focus decision: {} via {} -> {}:{} (local={})",
                    actionName(decision.action),
                    decision.edge ? edgeName(*decision.edge) : "<none>",
                    decision.target->screen.node,
                    decision.target->screen.index,
                    decision.target->local
                );
            }
            else {
                SPDLOG_INFO("Focus decision: {}", actionName(decision.action));
            }
        }
    }
    else {
        forwardToRemote = currentRemoteTarget().has_value();
        if (!forwardToRemote) {
            SPDLOG_INFO("Local event: {}", event);
        }
    }

    if (!forwardToRemote) {
        return;
    }

    auto remoteEvent = makeRemoteEvent(event, transitionEdge);
    if (!remoteEvent) {
        SPDLOG_WARN("Failed to map event to remote target: {}", event);
        return;
    }

    if (focusEntered) {
        publishFrame(proto::Frame {
            .type = proto::MessageType::FocusEnter,
            .payload = proto::FocusEnter {
                .screenIndex = remoteEvent->metadata.screenIndex,
                .edge = static_cast<uint8_t>(transitionEdge.value_or(Edge::Right)),
            },
        });
        beginRemotePointerCapture(event.metadata.screenIndex, transitionEdge.value_or(Edge::Right), *remoteEvent);
    }

    SPDLOG_DEBUG("Forwarding event to remote target {}: {}", remoteEvent->metadata.screenIndex, *remoteEvent);
    if (auto message = proto::toMessage(*remoteEvent)) {
        publishFrame(proto::Frame {.type = frameTypeForEventType(remoteEvent->type), .payload = std::move(*message)});
    }
}

auto HostApp::publishFrame(proto::Frame frame) -> void {
    if (!mActiveSender) {
        return;
    }
    if (auto sent = mActiveSender.trySend(std::move(frame)); !sent) {
        SPDLOG_WARN("Dropping outbound input frame because client queue is full or closed");
    }
}

auto HostApp::interceptLocalHotkey(const InputEvent &event) -> bool {
    if (event.type != InputEvent::Type::KeyPress) {
        return false;
    }

    auto data = event.getIf<InputEvent::KeyData>();
    if (data == nullptr || data->key != core::Key::Pause) {
        return false;
    }

    if (auto target = currentRemoteTarget()) {
        publishFrame(proto::Frame {
            .type = proto::MessageType::FocusLeave,
            .payload = proto::FocusLeave {.screenIndex = target->screen.index},
        });
        if (auto local = mTopology.localScreen(mRemoteCursor ? mRemoteCursor->localScreenIndex : 0); local) {
            mFocus.activateLocal(*local);
        }
        endRemotePointerCapture(true);
        SPDLOG_INFO("Manual focus return to local via Pause hotkey");
    }
    else {
        SPDLOG_INFO("Pause hotkey captured locally (reserved for dev focus control)");
    }
    return true;
}

auto HostApp::makeRemoteEvent(const InputEvent &event, std::optional<Edge> edge) const -> std::optional<InputEvent> {
    auto target = currentRemoteTarget();
    if (!target) {
        return std::nullopt;
    }

    auto remoteBounds = mTopology.screen(target->screen);
    if (remoteBounds == nullptr) {
        return std::nullopt;
    }

    auto mapped = event;
    mapped.metadata.screenIndex = target->screen.index;

    if (auto data = mapped.getIf<InputEvent::MouseMoveData>()) {
        data->x = clampCoord(data->x, remoteBounds->width);
        data->y = clampCoord(data->y, remoteBounds->height);

        if (edge) {
            auto local = mTopology.localScreen(event.metadata.screenIndex);
            auto localBounds = local ? mTopology.screen(*local) : nullptr;
            if (localBounds != nullptr) {
                switch (*edge) {
                    case Edge::Left:
                        data->x = remoteBounds->width - 1;
                        data->y = mapAxis(data->y, localBounds->height, remoteBounds->height);
                        break;
                    case Edge::Right:
                        data->x = 0;
                        data->y = mapAxis(data->y, localBounds->height, remoteBounds->height);
                        break;
                    case Edge::Top:
                        data->x = mapAxis(data->x, localBounds->width, remoteBounds->width);
                        data->y = remoteBounds->height - 1;
                        break;
                    case Edge::Bottom:
                        data->x = mapAxis(data->x, localBounds->width, remoteBounds->width);
                        data->y = 0;
                        break;
                }
            }
        }
        return mapped;
    }

    if (auto data = mapped.getIf<InputEvent::MouseButtonData>()) {
        data->x = clampCoord(data->x, remoteBounds->width);
        data->y = clampCoord(data->y, remoteBounds->height);
        return mapped;
    }

    if (auto data = mapped.getIf<InputEvent::MouseWheelData>()) {
        data->x = clampCoord(data->x, remoteBounds->width);
        data->y = clampCoord(data->y, remoteBounds->height);
        return mapped;
    }

    return mapped;
}

auto HostApp::currentRemoteTarget() const -> std::optional<FocusTarget> {
    auto target = mFocus.activeTarget();
    if (!target || target->local) {
        return std::nullopt;
    }
    return target;
}

auto HostApp::handleRemotePointerMotion(const InputEvent &event, const InputEvent::MouseMoveData &data) -> bool {
    if (!mRemoteCursor) {
        return false;
    }
    if (event.metadata.screenIndex != mRemoteCursor->localScreenIndex) {
        return false;
    }

    auto remoteId = ScreenId {mRemoteNode, mRemoteCursor->remoteScreenIndex};
    auto remoteBounds = mTopology.screen(remoteId);
    if (remoteBounds == nullptr) {
        return false;
    }

    auto dx = data.x - mRemoteCursor->anchorX;
    auto dy = data.y - mRemoteCursor->anchorY;
    if (dx == 0 && dy == 0) {
        return true;
    }

    mRemoteCursor->x = clampCoord(mRemoteCursor->x + dx, remoteBounds->width);
    mRemoteCursor->y = clampCoord(mRemoteCursor->y + dy, remoteBounds->height);

    auto remoteEvent = InputEvent {
        .type = InputEvent::Type::MouseMove,
        .metadata = {.screenIndex = mRemoteCursor->remoteScreenIndex},
        .payload = InputEvent::MouseMoveData {.x = mRemoteCursor->x, .y = mRemoteCursor->y},
    };

    if (auto message = proto::toMessage(remoteEvent)) {
        publishFrame(proto::Frame {.type = proto::MessageType::MouseMove, .payload = std::move(*message)});
    }
    warpLocalCursor(mRemoteCursor->localScreenIndex, mRemoteCursor->anchorX, mRemoteCursor->anchorY);
    return true;
}

auto HostApp::beginRemotePointerCapture(uint32_t localScreenIndex, Edge entryEdge, const InputEvent &remoteEvent) -> void {
    auto remoteData = remoteEvent.getIf<InputEvent::MouseMoveData>();
    auto localId = mTopology.localScreen(localScreenIndex);
    auto remoteId = ScreenId {mRemoteNode, remoteEvent.metadata.screenIndex};
    if (!remoteData || !localId) {
        return;
    }

    auto localBounds = mTopology.screen(*localId);
    auto remoteBounds = mTopology.screen(remoteId);
    if (localBounds == nullptr || remoteBounds == nullptr) {
        return;
    }

    mRemoteCursor = RemoteCursorState {
        .localScreenIndex = localScreenIndex,
        .remoteScreenIndex = remoteEvent.metadata.screenIndex,
        .entryEdge = entryEdge,
        .x = remoteData->x,
        .y = remoteData->y,
        .anchorX = clampCoord(localBounds->width / 2, localBounds->width),
        .anchorY = clampCoord(mapAxis(remoteData->y, remoteBounds->height, localBounds->height), localBounds->height),
    };

    SPDLOG_INFO(
        "Host remote capture armed: local screen {} anchor=({}, {}), remote screen {} pos=({}, {})",
        mRemoteCursor->localScreenIndex,
        mRemoteCursor->anchorX,
        mRemoteCursor->anchorY,
        mRemoteCursor->remoteScreenIndex,
        mRemoteCursor->x,
        mRemoteCursor->y
    );
    warpLocalCursor(mRemoteCursor->localScreenIndex, mRemoteCursor->anchorX, mRemoteCursor->anchorY);
}

auto HostApp::endRemotePointerCapture(bool restoreLocalCursor) -> void {
    if (!mRemoteCursor) {
        return;
    }

    auto state = *mRemoteCursor;
    mRemoteCursor.reset();

    if (!restoreLocalCursor) {
        return;
    }

    auto localId = mTopology.localScreen(state.localScreenIndex);
    auto remoteId = ScreenId {mRemoteNode, state.remoteScreenIndex};
    auto localBounds = localId ? mTopology.screen(*localId) : nullptr;
    auto remoteBounds = mTopology.screen(remoteId);
    if (localBounds == nullptr || remoteBounds == nullptr) {
        return;
    }

    auto x = 0;
    auto y = 0;
    switch (state.entryEdge) {
        case Edge::Left:
            x = 0;
            y = mapAxis(state.y, remoteBounds->height, localBounds->height);
            break;
        case Edge::Right:
            x = localBounds->width - 1;
            y = mapAxis(state.y, remoteBounds->height, localBounds->height);
            break;
        case Edge::Top:
            x = mapAxis(state.x, remoteBounds->width, localBounds->width);
            y = 0;
            break;
        case Edge::Bottom:
            x = mapAxis(state.x, remoteBounds->width, localBounds->width);
            y = localBounds->height - 1;
            break;
    }
    warpLocalCursor(state.localScreenIndex, x, y);
}

auto HostApp::warpLocalCursor(uint32_t screenIndex, int32_t x, int32_t y) -> void {
    if (!mInjector) {
        return;
    }

    auto move = InputEvent {
        .type = InputEvent::Type::MouseMove,
        .metadata = {.screenIndex = screenIndex, .injected = true},
        .payload = InputEvent::MouseMoveData {.x = x, .y = y},
    };
    if (auto error = mInjector->sendEventsSync(std::span<const InputEvent>(&move, 1)); error) {
        SPDLOG_WARN("Failed to warp local cursor: {}", error.message());
    }
}

auto HostApp::reloadLocalTopology() -> void {
    rebuildTopology();
}

auto HostApp::updateRemoteTopology(std::string nodeName, std::span<const platform::ScreenInfo> screens) -> void {
    mRemoteNode = std::move(nodeName);
    mRemoteScreens.assign(screens.begin(), screens.end());
    rebuildTopology();
}

auto HostApp::clearRemoteTopology() -> void {
    endRemotePointerCapture(false);
    if (mRemoteNode.empty() && mRemoteScreens.empty()) {
        return;
    }

    mRemoteNode.clear();
    mRemoteScreens.clear();
    rebuildTopology();

    if (auto screen = mTopology.localScreen(0); screen) {
        mFocus.activateLocal(*screen);
    }
    SPDLOG_INFO("Remote topology cleared");
}

auto HostApp::rebuildTopology() -> void {
    auto localScreens = mPlatform ? mPlatform->screens() : std::vector<platform::ScreenInfo> {};

    mTopology.clear();
    mTopology.setLocalNode("local");
    mTopology.loadLocalScreens(localScreens);

    auto localOrdered = orderedScreenIds(localScreens, mTopology.localNode());
    linkOrderedScreens(localOrdered, Edge::Right);

    if (!mRemoteNode.empty() && !mRemoteScreens.empty()) {
        for (size_t i = 0; i < mRemoteScreens.size(); ++i) {
            const auto &screen = mRemoteScreens[i];
            mTopology.upsertScreen(
                ScreenId {mRemoteNode, static_cast<uint32_t>(i)},
                ScreenBounds {.x = screen.x, .y = screen.y, .width = screen.width, .height = screen.height}
            );
        }

        auto remoteOrdered = orderedScreenIds(mRemoteScreens, mRemoteNode);
        linkOrderedScreens(remoteOrdered, Edge::Right);

        if (!localOrdered.empty() && !remoteOrdered.empty()) {
            mTopology.setLink(localOrdered.back(), Edge::Right, remoteOrdered.front(), true);
        }
    }

    if (auto active = mFocus.activeTarget(); active) {
        if (mTopology.screen(active->screen) == nullptr) {
            if (auto screen = mTopology.localScreen(0); screen) {
                mFocus.activateLocal(*screen);
            }
        }
    }
    else if (auto screen = mTopology.localScreen(0); screen) {
        mFocus.activateLocal(*screen);
    }
}

auto HostApp::linkOrderedScreens(std::span<const ScreenId> ordered, Edge edge) -> void {
    for (size_t i = 1; i < ordered.size(); ++i) {
        mTopology.setLink(ordered[i - 1], edge, ordered[i], true);
    }
}

} // namespace mksync::app
