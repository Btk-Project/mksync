#include "client.hpp"

#include <spdlog/spdlog.h>

#include "../proto/message.hpp"

namespace mksync::app {
namespace {
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

auto edgeName(domain::Edge edge) -> const char * {
    switch (edge) {
        case domain::Edge::Left: return "Left";
        case domain::Edge::Right: return "Right";
        case domain::Edge::Top: return "Top";
        case domain::Edge::Bottom: return "Bottom";
    }
    return "Unknown";
}

auto opposite(domain::Edge edge) -> domain::Edge {
    return domain::ScreenTopology::opposite(edge);
}
} // namespace

auto ClientApp::run(const ilias::IPEndpoint &endpoint) -> ilias::IoTask<void> {
    if (auto init = co_await initialize(); !init) {
        co_return init;
    }

    auto tcp = co_await ilias::TcpStream::connect(endpoint);
    if (!tcp) {
        co_await shutdown();
        co_return ilias::Err(tcp.error());
    }

    auto screens = mPlatform->screens();

    SPDLOG_INFO("Client connected to {}", endpoint);
    transport::BufferedTcpStream stream {std::move(*tcp)};

    if (auto written = co_await transport::writeFrame(stream, transport::makeHello("mksync-client", static_cast<uint32_t>(screens.size()))); !written) {
        co_await shutdown();
        co_return written;
    }

    auto ack = co_await transport::readFrame(stream);
    if (!ack) {
        co_await shutdown();
        co_return ilias::Err(ack.error());
    }
    if (ack->type != proto::MessageType::HelloAck) {
        co_await shutdown();
        co_return ilias::Err(make_error_code(std::errc::protocol_error));
    }

    auto helloAck = std::get_if<proto::HelloAck>(&ack->payload);
    if (helloAck == nullptr) {
        co_await shutdown();
        co_return ilias::Err(make_error_code(std::errc::protocol_error));
    }
    SPDLOG_INFO("Client received HelloAck, remote screens={}", helloAck->screenCount);

    for (uint32_t i = 0; i < helloAck->screenCount; ++i) {
        auto screenFrame = co_await transport::readFrame(stream);
        if (!screenFrame) {
            co_await shutdown();
            co_return ilias::Err(screenFrame.error());
        }
        if (screenFrame->type != proto::MessageType::ScreenInfo) {
            co_await shutdown();
            co_return ilias::Err(make_error_code(std::errc::protocol_error));
        }

        auto screen = std::get_if<proto::ScreenInfo>(&screenFrame->payload);
        if (screen == nullptr) {
            co_await shutdown();
            co_return ilias::Err(make_error_code(std::errc::protocol_error));
        }

        SPDLOG_INFO(
            "Received remote screen {} '{}': pos=({}, {}), size={}x{}",
            screen->index,
            screen->name,
            screen->x,
            screen->y,
            screen->width,
            screen->height
        );
    }

    for (uint32_t i = 0; i < screens.size(); ++i) {
        if (auto written = co_await transport::writeFrame(stream, makeScreenInfoFrame(i, screens[i])); !written) {
            co_await shutdown();
            co_return written;
        }
    }
    SPDLOG_INFO("Client announced {} screen(s)", screens.size());

    auto pair = ilias::mpsc::channel<proto::Frame>(128);
    auto [readResult, writeResult, captureResult] = co_await ilias::finally(
        ilias::whenAny(
            sessionReadLoop(stream, pair.sender),
            sessionWriteLoop(stream, std::move(pair.receiver)),
            captureLoop(pair.sender)
        ),
        [this]() -> ilias::Task<void> {
            co_await shutdown();
        }
    );

    if (readResult && !*readResult) {
        co_return ilias::Err(readResult->error());
    }
    if (writeResult && !*writeResult) {
        co_return ilias::Err(writeResult->error());
    }
    if (captureResult && !*captureResult) {
        co_return ilias::Err(captureResult->error());
    }
    co_return {};
}

auto ClientApp::initialize() -> ilias::IoTask<void> {
    mPlatform = platform::createPlatform();
    if (!mPlatform) {
        co_return ilias::Err(make_error_code(std::errc::not_supported));
    }

    mCapture = mPlatform->createInputCapture();
    mInjector = mPlatform->createInputInjector();
    if (!mCapture || !mInjector) {
        co_return ilias::Err(make_error_code(std::errc::not_supported));
    }

    if (auto result = co_await mCapture->initialize(); !result) {
        co_return result;
    }

    auto result = co_await mInjector->initialize();
    if (result) {
        SPDLOG_INFO("Client injector initialized");
    }
    co_return result;
}

auto ClientApp::shutdown() -> ilias::Task<void> {
    clearRemoteFocus();
    if (mCapture) {
        co_await mCapture->shutdown();
    }
    if (mInjector) {
        co_await mInjector->shutdown();
    }
    SPDLOG_INFO("ClientApp shutdown complete");
    co_return;
}

auto ClientApp::sessionReadLoop(transport::BufferedTcpStream &stream, ilias::mpsc::Sender<proto::Frame> sender) -> ilias::IoTask<void> {
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
                }
                break;
            case proto::MessageType::FocusEnter: {
                auto focus = std::get_if<proto::FocusEnter>(&frame->payload);
                if (focus == nullptr) {
                    co_return ilias::Err(make_error_code(std::errc::protocol_error));
                }
                auto hostEdge = static_cast<domain::Edge>(focus->edge);
                auto localEntryEdge = opposite(hostEdge);
                setRemoteFocus(focus->screenIndex, localEntryEdge);
                SPDLOG_INFO(
                    "Remote focus entered screen {} via local edge {} (host edge {})",
                    focus->screenIndex,
                    edgeName(localEntryEdge),
                    edgeName(hostEdge)
                );
                break;
            }
            case proto::MessageType::FocusLeave: {
                auto focus = std::get_if<proto::FocusLeave>(&frame->payload);
                if (focus == nullptr) {
                    co_return ilias::Err(make_error_code(std::errc::protocol_error));
                }
                SPDLOG_INFO("Remote focus left screen {} by host request", focus->screenIndex);
                clearRemoteFocus();
                break;
            }
            case proto::MessageType::ScreenInfo:
                if (auto screen = std::get_if<proto::ScreenInfo>(&frame->payload)) {
                    SPDLOG_INFO(
                        "Received remote screen {} '{}': pos=({}, {}), size={}x{}",
                        screen->index,
                        screen->name,
                        screen->x,
                        screen->y,
                        screen->width,
                        screen->height
                    );
                }
                break;
            default:
                if (auto event = proto::toInputEvent(frame->type, frame->payload); event) {
                    SPDLOG_DEBUG(
                        "Client injecting frame {} (remote_focus_active={}, screen={})",
                        frame->type,
                        mRemoteFocusActive,
                        mRemoteFocusScreen.value_or(0)
                    );
                    if (auto sent = co_await mInjector->sendEvents(std::span<const platform::InputEvent>(&*event, 1)); !sent) {
                        co_return sent;
                    }
                }
                else {
                    SPDLOG_DEBUG("Ignoring unsupported frame type {}", frame->type);
                }
                break;
        }
    }
}

auto ClientApp::sessionWriteLoop(transport::BufferedTcpStream &stream, ilias::mpsc::Receiver<proto::Frame> receiver) -> ilias::IoTask<void> {
    while (true) {
        auto frame = co_await receiver.recv();
        if (!frame) {
            co_return ilias::Err(make_error_code(std::errc::operation_canceled));
        }
        SPDLOG_DEBUG("Client sending frame {}", frame->type);
        if (auto written = co_await transport::writeFrame(stream, *frame); !written) {
            co_return written;
        }
    }
}

auto ClientApp::captureLoop(ilias::mpsc::Sender<proto::Frame> sender) -> ilias::IoTask<void> {
    while (true) {
        auto event = co_await mCapture->nextEvent();
        if (!event) {
            co_return ilias::Err(event.error());
        }
        handleCapturedEvent(*event, sender);
    }
}

auto ClientApp::handleCapturedEvent(const platform::InputEvent &event, ilias::mpsc::Sender<proto::Frame> sender) -> void {
    if (!mRemoteFocusActive) {
        return;
    }

    if (event.type == platform::InputEvent::Type::KeyPress && !event.metadata.injected) {
        auto data = event.getIf<platform::InputEvent::KeyData>();
        if (data && data->key == core::Key::Pause) {
            if (auto screenIndex = mRemoteFocusScreen) {
                if (auto queued = sender.trySend(proto::Frame {
                    .type = proto::MessageType::FocusLeave,
                    .payload = proto::FocusLeave {.screenIndex = *screenIndex},
                }); !queued) {
                    SPDLOG_WARN("Client failed to queue FocusLeave by Pause hotkey");
                }
                else {
                    SPDLOG_INFO("Client requested focus leave via local Pause hotkey");
                }
            }
            clearRemoteFocus();
        }
        return;
    }

    if (event.type != platform::InputEvent::Type::MouseMove || !event.metadata.injected) {
        return;
    }

    if (!mRemoteFocusScreen || !mRemoteEntryEdge) {
        return;
    }

    if (event.metadata.screenIndex != *mRemoteFocusScreen) {
        return;
    }

    auto data = event.getIf<platform::InputEvent::MouseMoveData>();
    if (data == nullptr) {
        return;
    }

    auto screens = mPlatform->screens();
    if (*mRemoteFocusScreen >= screens.size()) {
        return;
    }

    auto returnEdge = *mRemoteEntryEdge;
    const auto &screen = screens[*mRemoteFocusScreen];

    if (!mReturnEdgeArmed) {
        if (movedAwayFromReturnEdge(*data, screen, returnEdge)) {
            mReturnEdgeArmed = true;
            SPDLOG_INFO("Client armed remote return detection on edge {}", edgeName(returnEdge));
        }
        return;
    }

    if (!shouldReturnToHost(*data, screen, returnEdge)) {
        return;
    }

    if (auto queued = sender.trySend(proto::Frame {
        .type = proto::MessageType::FocusLeave,
        .payload = proto::FocusLeave {.screenIndex = *mRemoteFocusScreen},
    }); !queued) {
        SPDLOG_WARN("Client failed to queue FocusLeave on edge {}", edgeName(returnEdge));
        return;
    }

    SPDLOG_INFO("Client requested focus leave from screen {} via edge {}", *mRemoteFocusScreen, edgeName(returnEdge));
    clearRemoteFocus();
}

auto ClientApp::clearRemoteFocus() -> void {
    mRemoteFocusActive = false;
    mRemoteFocusScreen.reset();
    mRemoteEntryEdge.reset();
    mReturnEdgeArmed = false;
}

auto ClientApp::setRemoteFocus(uint32_t screenIndex, domain::Edge entryEdge) -> void {
    mRemoteFocusActive = true;
    mRemoteFocusScreen = screenIndex;
    mRemoteEntryEdge = entryEdge;
    mReturnEdgeArmed = false;
}


auto ClientApp::shouldReturnToHost(const platform::InputEvent::MouseMoveData &data, const platform::ScreenInfo &screen, domain::Edge edge) const -> bool {
    switch (edge) {
        case domain::Edge::Left: return data.x <= mReturnEdgeThreshold;
        case domain::Edge::Right: return data.x >= screen.width - 1 - mReturnEdgeThreshold;
        case domain::Edge::Top: return data.y <= mReturnEdgeThreshold;
        case domain::Edge::Bottom: return data.y >= screen.height - 1 - mReturnEdgeThreshold;
    }
    return false;
}

auto ClientApp::movedAwayFromReturnEdge(const platform::InputEvent::MouseMoveData &data, const platform::ScreenInfo &screen, domain::Edge edge) const -> bool {
    switch (edge) {
        case domain::Edge::Left: return data.x > mReturnEdgeThreshold;
        case domain::Edge::Right: return data.x < screen.width - 1 - mReturnEdgeThreshold;
        case domain::Edge::Top: return data.y > mReturnEdgeThreshold;
        case domain::Edge::Bottom: return data.y < screen.height - 1 - mReturnEdgeThreshold;
    }
    return false;
}

} // namespace mksync::app
