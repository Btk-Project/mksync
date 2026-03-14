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

    co_return co_await ilias::finally(
        sessionLoop(stream),
        [this]() -> ilias::Task<void> {
            co_await shutdown();
        }
    );
}

auto ClientApp::initialize() -> ilias::IoTask<void> {
    mPlatform = platform::createPlatform();
    if (!mPlatform) {
        co_return ilias::Err(make_error_code(std::errc::not_supported));
    }

    mInjector = mPlatform->createInputInjector();
    if (!mInjector) {
        co_return ilias::Err(make_error_code(std::errc::not_supported));
    }

    auto result = co_await mInjector->initialize();
    if (result) {
        SPDLOG_INFO("Client injector initialized");
    }
    co_return result;
}

auto ClientApp::shutdown() -> ilias::Task<void> {
    if (mInjector) {
        co_await mInjector->shutdown();
    }
    SPDLOG_INFO("ClientApp shutdown complete");
    co_return;
}

auto ClientApp::sessionLoop(transport::BufferedTcpStream &stream) -> ilias::IoTask<void> {
    while (true) {
        auto frame = co_await transport::readFrame(stream);
        if (!frame) {
            co_return ilias::Err(frame.error());
        }

        if (frame->type == proto::MessageType::Ping) {
            if (auto ping = std::get_if<proto::Ping>(&frame->payload)) {
                auto pong = transport::makePong(ping->timestampUs);
                if (auto written = co_await transport::writeFrame(stream, pong); !written) {
                    co_return written;
                }
            }
            continue;
        }

        if (frame->type == proto::MessageType::ScreenInfo) {
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
            continue;
        }

        if (auto event = proto::toInputEvent(frame->type, frame->payload); event) {
            SPDLOG_DEBUG("Client injecting frame {}", frame->type);
            if (auto sent = co_await mInjector->sendEvents(std::span<const platform::InputEvent>(&*event, 1)); !sent) {
                co_return sent;
            }
        }
        else {
            SPDLOG_DEBUG("Ignoring unsupported frame type {}", frame->type);
        }
    }
}

} // namespace mksync::app
