#include "pch.hpp"
#include "codec.hpp"

#include <bit>
#include <cstring>

namespace mksync::proto {
namespace {
class Writer {
public:
    auto writeU8(uint8_t value) -> void { mBytes.push_back(std::byte {value}); }
    auto writeBool(bool value) -> void { writeU8(value ? 1 : 0); }
    auto writeU16(uint16_t value) -> void {
        writePod<uint16_t>(value);
    }
    auto writeU32(uint32_t value) -> void {
        writePod<uint32_t>(value);
    }
    auto writeU64(uint64_t value) -> void {
        writePod<uint64_t>(value);
    }
    auto writeI32(int32_t value) -> void {
        writePod<int32_t>(value);
    }
    auto writeString(const std::string &value) -> void {
        writeU32(static_cast<uint32_t>(value.size()));
        mBytes.insert(mBytes.end(), reinterpret_cast<const std::byte *>(value.data()), reinterpret_cast<const std::byte *>(value.data() + value.size()));
    }
    auto bytes() && -> std::vector<std::byte> { return std::move(mBytes); }
private:
    template <typename T>
    auto writePod(T value) -> void {
        auto raw = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
        mBytes.insert(mBytes.end(), raw.begin(), raw.end());
    }
    std::vector<std::byte> mBytes;
};

class Reader {
public:
    explicit Reader(std::span<const std::byte> bytes) : mBytes(bytes) {}

    template <typename T>
    auto readPod() -> std::expected<T, std::string> {
        if (mOffset + sizeof(T) > mBytes.size()) {
            return std::unexpected("frame truncated");
        }
        std::array<std::byte, sizeof(T)> raw {};
        std::memcpy(raw.data(), mBytes.data() + mOffset, sizeof(T));
        mOffset += sizeof(T);
        return std::bit_cast<T>(raw);
    }

    auto readBool() -> std::expected<bool, std::string> {
        auto value = readPod<uint8_t>();
        if (!value) {
            return std::unexpected(value.error());
        }
        return *value != 0;
    }

    auto readString() -> std::expected<std::string, std::string> {
        auto size = readPod<uint32_t>();
        if (!size) {
            return std::unexpected(size.error());
        }
        if (mOffset + *size > mBytes.size()) {
            return std::unexpected("invalid string size");
        }
        auto begin = reinterpret_cast<const char *>(mBytes.data() + mOffset);
        std::string value(begin, begin + *size);
        mOffset += *size;
        return value;
    }

    auto consumedAll() const -> bool { return mOffset == mBytes.size(); }

private:
    std::span<const std::byte> mBytes;
    size_t mOffset = 0;
};

auto encodePayload(MessageType type, const Message &message) -> std::vector<std::byte> {
    Writer writer;
    switch (type) {
        case MessageType::Hello: {
            const auto &value = std::get<Hello>(message);
            writer.writeU32(value.version);
            writer.writeU32(value.screenCount);
            writer.writeString(value.deviceName);
            break;
        }
        case MessageType::HelloAck: {
            const auto &value = std::get<HelloAck>(message);
            writer.writeU32(value.version);
            writer.writeU32(value.screenCount);
            break;
        }
        case MessageType::Ping: {
            const auto &value = std::get<Ping>(message);
            writer.writeU64(value.timestampUs);
            break;
        }
        case MessageType::Pong: {
            const auto &value = std::get<Pong>(message);
            writer.writeU64(value.timestampUs);
            break;
        }
        case MessageType::ScreenInfo: {
            const auto &value = std::get<ScreenInfo>(message);
            writer.writeU32(value.index);
            writer.writeI32(value.x);
            writer.writeI32(value.y);
            writer.writeI32(value.width);
            writer.writeI32(value.height);
            writer.writeI32(value.dpi);
            writer.writeString(value.name);
            writer.writeBool(value.primary);
            break;
        }
        case MessageType::FocusEnter: {
            const auto &value = std::get<FocusEnter>(message);
            writer.writeU32(value.screenIndex);
            writer.writeU8(value.edge);
            break;
        }
        case MessageType::FocusLeave: {
            const auto &value = std::get<FocusLeave>(message);
            writer.writeU32(value.screenIndex);
            break;
        }
        case MessageType::MouseMove: {
            const auto &value = std::get<MouseMove>(message);
            writer.writeU32(value.screenIndex);
            writer.writeI32(value.x);
            writer.writeI32(value.y);
            break;
        }
        case MessageType::MousePress:
        case MessageType::MouseRelease: {
            const auto &value = std::get<MouseButton>(message);
            writer.writeU32(value.screenIndex);
            writer.writeI32(value.x);
            writer.writeI32(value.y);
            writer.writeU8(static_cast<uint8_t>(value.button));
            break;
        }
        case MessageType::MouseWheel: {
            const auto &value = std::get<MouseWheel>(message);
            writer.writeU32(value.screenIndex);
            writer.writeI32(value.x);
            writer.writeI32(value.y);
            writer.writeI32(value.deltaX);
            writer.writeI32(value.deltaY);
            break;
        }
        case MessageType::KeyPress:
        case MessageType::KeyRelease: {
            const auto &value = std::get<KeyEvent>(message);
            writer.writeU32(static_cast<uint32_t>(value.key));
            writer.writeU8(static_cast<uint8_t>(value.modifiers));
            writer.writeU32(value.nativeCode);
            writer.writeBool(value.repeat);
            break;
        }
        case MessageType::Error: {
            const auto &value = std::get<Error>(message);
            writer.writeString(value.message);
            break;
        }
    }
    return std::move(writer).bytes();
}

auto decodePayload(MessageType type, std::span<const std::byte> payload) -> std::expected<Message, std::string> {
    Reader reader(payload);

    auto expectConsumed = [&](Message message) -> std::expected<Message, std::string> {
        if (!reader.consumedAll()) {
            return std::unexpected("unexpected trailing bytes");
        }
        return message;
    };

    switch (type) {
        case MessageType::Hello: {
            auto version = reader.readPod<uint32_t>();
            auto screenCount = reader.readPod<uint32_t>();
            auto deviceName = reader.readString();
            if (!version) return std::unexpected(version.error());
            if (!screenCount) return std::unexpected(screenCount.error());
            if (!deviceName) return std::unexpected(deviceName.error());
            return expectConsumed(Hello {.version = *version, .screenCount = *screenCount, .deviceName = std::move(*deviceName)});
        }
        case MessageType::HelloAck: {
            auto version = reader.readPod<uint32_t>();
            auto screenCount = reader.readPod<uint32_t>();
            if (!version) return std::unexpected(version.error());
            if (!screenCount) return std::unexpected(screenCount.error());
            return expectConsumed(HelloAck {.version = *version, .screenCount = *screenCount});
        }
        case MessageType::Ping: {
            auto timestampUs = reader.readPod<uint64_t>();
            if (!timestampUs) return std::unexpected(timestampUs.error());
            return expectConsumed(Ping {.timestampUs = *timestampUs});
        }
        case MessageType::Pong: {
            auto timestampUs = reader.readPod<uint64_t>();
            if (!timestampUs) return std::unexpected(timestampUs.error());
            return expectConsumed(Pong {.timestampUs = *timestampUs});
        }
        case MessageType::ScreenInfo: {
            auto index = reader.readPod<uint32_t>();
            auto x = reader.readPod<int32_t>();
            auto y = reader.readPod<int32_t>();
            auto width = reader.readPod<int32_t>();
            auto height = reader.readPod<int32_t>();
            auto dpi = reader.readPod<int32_t>();
            auto name = reader.readString();
            auto primary = reader.readBool();
            if (!index) return std::unexpected(index.error());
            if (!x) return std::unexpected(x.error());
            if (!y) return std::unexpected(y.error());
            if (!width) return std::unexpected(width.error());
            if (!height) return std::unexpected(height.error());
            if (!dpi) return std::unexpected(dpi.error());
            if (!name) return std::unexpected(name.error());
            if (!primary) return std::unexpected(primary.error());
            return expectConsumed(ScreenInfo {.index = *index, .x = *x, .y = *y, .width = *width, .height = *height, .dpi = *dpi, .name = std::move(*name), .primary = *primary});
        }
        case MessageType::FocusEnter: {
            auto screenIndex = reader.readPod<uint32_t>();
            auto edge = reader.readPod<uint8_t>();
            if (!screenIndex) return std::unexpected(screenIndex.error());
            if (!edge) return std::unexpected(edge.error());
            return expectConsumed(FocusEnter {.screenIndex = *screenIndex, .edge = *edge});
        }
        case MessageType::FocusLeave: {
            auto screenIndex = reader.readPod<uint32_t>();
            if (!screenIndex) return std::unexpected(screenIndex.error());
            return expectConsumed(FocusLeave {.screenIndex = *screenIndex});
        }
        case MessageType::MouseMove: {
            auto screenIndex = reader.readPod<uint32_t>();
            auto x = reader.readPod<int32_t>();
            auto y = reader.readPod<int32_t>();
            if (!screenIndex) return std::unexpected(screenIndex.error());
            if (!x) return std::unexpected(x.error());
            if (!y) return std::unexpected(y.error());
            return expectConsumed(MouseMove {.screenIndex = *screenIndex, .x = *x, .y = *y});
        }
        case MessageType::MousePress:
        case MessageType::MouseRelease: {
            auto screenIndex = reader.readPod<uint32_t>();
            auto x = reader.readPod<int32_t>();
            auto y = reader.readPod<int32_t>();
            auto button = reader.readPod<uint8_t>();
            if (!screenIndex) return std::unexpected(screenIndex.error());
            if (!x) return std::unexpected(x.error());
            if (!y) return std::unexpected(y.error());
            if (!button) return std::unexpected(button.error());
            return expectConsumed(MouseButton {.screenIndex = *screenIndex, .x = *x, .y = *y, .button = static_cast<core::MouseButton>(*button)});
        }
        case MessageType::MouseWheel: {
            auto screenIndex = reader.readPod<uint32_t>();
            auto x = reader.readPod<int32_t>();
            auto y = reader.readPod<int32_t>();
            auto deltaX = reader.readPod<int32_t>();
            auto deltaY = reader.readPod<int32_t>();
            if (!screenIndex) return std::unexpected(screenIndex.error());
            if (!x) return std::unexpected(x.error());
            if (!y) return std::unexpected(y.error());
            if (!deltaX) return std::unexpected(deltaX.error());
            if (!deltaY) return std::unexpected(deltaY.error());
            return expectConsumed(MouseWheel {.screenIndex = *screenIndex, .x = *x, .y = *y, .deltaX = *deltaX, .deltaY = *deltaY});
        }
        case MessageType::KeyPress:
        case MessageType::KeyRelease: {
            auto key = reader.readPod<uint32_t>();
            auto modifiers = reader.readPod<uint8_t>();
            auto nativeCode = reader.readPod<uint32_t>();
            auto repeat = reader.readBool();
            if (!key) return std::unexpected(key.error());
            if (!modifiers) return std::unexpected(modifiers.error());
            if (!nativeCode) return std::unexpected(nativeCode.error());
            if (!repeat) return std::unexpected(repeat.error());
            return expectConsumed(KeyEvent {.key = static_cast<core::Key>(*key), .modifiers = static_cast<core::KeyModifier>(*modifiers), .nativeCode = *nativeCode, .repeat = *repeat});
        }
        case MessageType::Error: {
            auto error = reader.readString();
            if (!error) return std::unexpected(error.error());
            return expectConsumed(Error {.message = std::move(*error)});
        }
    }
    return std::unexpected("unknown message type");
}
} // namespace

auto encodeFrame(const Frame &frame) -> std::vector<std::byte> {
    auto payload = encodePayload(frame.type, frame.payload);
    Writer writer;
    writer.writeU32(FrameHeader::Magic);
    writer.writeU16(1);
    writer.writeU16(static_cast<uint16_t>(frame.type));
    writer.writeU32(static_cast<uint32_t>(payload.size()));
    auto bytes = std::move(writer).bytes();
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    return bytes;
}

auto decodeFrame(std::span<const std::byte> bytes) -> std::expected<Frame, std::string> {
    Reader reader(bytes);
    auto magic = reader.readPod<uint32_t>();
    auto version = reader.readPod<uint16_t>();
    auto type = reader.readPod<uint16_t>();
    auto payloadSize = reader.readPod<uint32_t>();
    if (!magic) return std::unexpected(magic.error());
    if (!version) return std::unexpected(version.error());
    if (!type) return std::unexpected(type.error());
    if (!payloadSize) return std::unexpected(payloadSize.error());
    if (*magic != FrameHeader::Magic) {
        return std::unexpected("invalid frame magic");
    }
    if (*version != 1) {
        return std::unexpected("unsupported protocol version");
    }

    constexpr size_t headerSize = sizeof(uint32_t) + sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint32_t);
    if (bytes.size() != headerSize + *payloadSize) {
        return std::unexpected("frame size mismatch");
    }

    auto payload = bytes.subspan(headerSize, *payloadSize);
    auto message = decodePayload(static_cast<MessageType>(*type), payload);
    if (!message) {
        return std::unexpected(message.error());
    }
    return Frame {.type = static_cast<MessageType>(*type), .payload = std::move(*message)};
}

} // namespace mksync::proto



