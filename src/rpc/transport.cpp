#include "transport.hpp"
#include <ilias/io.hpp>

MKS_BEGIN

THIS_ERROR_IMPL(RpcError);

RpcTransport::RpcTransport(ilias::DynStream stream) : mStream(std::move(stream)) {
    
}

// TODO: Use serialization library
auto RpcTransport::writeMessage(const RpcMessage &message) -> IoTask<void> {
    if (auto hello = std::get_if<HelloMessage>(&message)) {
        size_t size = sizeof(hello->version) + sizeof(uint16_t) + hello->name.size();

        ILIAS_CO_TRYV(co_await writeHeader(size, MessageId::Hello));
        ILIAS_CO_TRYV(co_await mStream.writeU16Be(hello->version));
        ILIAS_CO_TRYV(co_await mStream.writeU16Be(hello->name.size()));
        ILIAS_CO_TRYV(co_await mStream.writeString(hello->name));
    }
    else if (auto screens = std::get_if<ScreensMessage>(&message)) {
        // int32_t x = 0;
        // int32_t y = 0;
        // int32_t width = 0;
        // int32_t height = 0;
        // int32_t dpi = 72;
        // std::string name;
        // bool primary = false;
        size_t size = sizeof(uint16_t); // Num of screens
        for (auto &screen : screens->screens) {
            size += sizeof(uint32_t) * 5 + sizeof(uint16_t) + sizeof(uint8_t) + screen.name.size();
        }

        ILIAS_CO_TRYV(co_await writeHeader(size, MessageId::Screens));
        ILIAS_CO_TRYV(co_await mStream.writeU16Be(screens->screens.size()));
        for (auto &screen : screens->screens) {
            ILIAS_CO_TRYV(co_await mStream.writeI32Be(screen.x));
            ILIAS_CO_TRYV(co_await mStream.writeI32Be(screen.y));
            ILIAS_CO_TRYV(co_await mStream.writeI32Be(screen.width));
            ILIAS_CO_TRYV(co_await mStream.writeI32Be(screen.height));
            ILIAS_CO_TRYV(co_await mStream.writeI32Be(screen.dpi));
            ILIAS_CO_TRYV(co_await mStream.writeU16Be(screen.name.size()));
            ILIAS_CO_TRYV(co_await mStream.writeString(screen.name));
            ILIAS_CO_TRYV(co_await mStream.writeU8(screen.primary));
        }
    }
    else if (auto ping = std::get_if<PingMessage>(&message)) {
        ILIAS_CO_TRYV(co_await writeHeader(0, MessageId::Ping));
    }
    else if (auto pong = std::get_if<PongMessage>(&message)) {
        ILIAS_CO_TRYV(co_await writeHeader(0, MessageId::Pong));
    }
    else {
        SPDLOG_ERROR("RpcTransport::writeMessage: Unknown message type");
        co_return Err(RpcError::UnknownMessageType);
    }
    ILIAS_CO_TRYV(co_await mStream.flush());
    co_return {};
}

auto RpcTransport::readMessage() -> IoTask<RpcMessage> {
    ILIAS_CO_TRY(auto header, co_await readHeader());
    auto [size, id] = header;
    switch (id) {
        case MessageId::Hello: {
            ILIAS_CO_TRY(auto version, co_await mStream.readU16Be());
            ILIAS_CO_TRY(auto nameSize, co_await mStream.readU16Be());

            std::string name;
            name.resize(nameSize);
            auto buf = ilias::makeBuffer(name);
            ILIAS_CO_TRYV(co_await mStream.readAll(buf));
            co_return RpcMessage {HelloMessage {version, std::move(name)}};
        }
        case MessageId::Screens: {
            ILIAS_CO_TRY(auto numScreens, co_await mStream.readU16Be());
            std::vector<ScreenInfo> screens;
            screens.reserve(numScreens);
            for (size_t i = 0; i < numScreens; ++i) {
                ILIAS_CO_TRY(auto x, co_await mStream.readI32Be());
                ILIAS_CO_TRY(auto y, co_await mStream.readI32Be());
                ILIAS_CO_TRY(auto width, co_await mStream.readI32Be());
                ILIAS_CO_TRY(auto height, co_await mStream.readI32Be());
                ILIAS_CO_TRY(auto dpi, co_await mStream.readI32Be());
                ILIAS_CO_TRY(auto nameSize, co_await mStream.readU16Be());

                std::string name;
                name.resize(nameSize);
                auto buf = ilias::makeBuffer(name);
                ILIAS_CO_TRYV(co_await mStream.readAll(buf));
                ILIAS_CO_TRY(auto primary, co_await mStream.readU8());
                screens.emplace_back(ScreenInfo {
                    .x = x,
                    .y = y,
                    .width = width,
                    .height = height,
                    .dpi = dpi,
                    .name = std::move(name),
                    .primary = static_cast<bool>(primary)
                });
            }
            co_return RpcMessage {ScreensMessage {std::move(screens)}};
        }
        case MessageId::Ping: {
            co_return RpcMessage {PingMessage {} };
        }
        case MessageId::Pong: {
            co_return RpcMessage {PongMessage {} };
        }
        default: {
            SPDLOG_ERROR("RpcTransport::readMessage: Unknown message id {}", id);
            co_return Err(RpcError::UnknownMessageType);
        }
    }
}

// Header ...
auto RpcTransport::writeHeader(uint16_t size, MessageId id) -> IoTask<void> {
    ILIAS_CO_TRYV(co_await mStream.writeU16Be(size));
    ILIAS_CO_TRYV(co_await mStream.writeU16Be(static_cast<uint16_t>(id)));
    co_return {};
}

auto RpcTransport::readHeader() -> IoTask<std::pair<uint16_t, MessageId> > {
    ILIAS_CO_TRY(auto size, co_await mStream.readU16Be());
    ILIAS_CO_TRY(auto id, co_await mStream.readU16Be());
    co_return std::pair {size, static_cast<MessageId>(id)};
}

MKS_END