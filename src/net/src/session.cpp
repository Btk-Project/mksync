#include "pch.hpp"
#include "session.hpp"

#include <array>
#include <vector>

namespace mksync::transport {
namespace {
constexpr size_t HeaderSize = sizeof(uint32_t) + sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint32_t);
}

auto writeFrame(BufferedTcpStream &stream, const proto::Frame &frame) -> ilias::IoTask<void> {
    auto encoded = proto::encodeFrame(frame);
    auto result = co_await stream.writeAll(ilias::makeBuffer(encoded));
    if (!result) {
        co_return ilias::Err(result.error());
    }
    auto flushed = co_await stream.flush();
    if (!flushed) {
        co_return ilias::Err(flushed.error());
    }
    co_return {};
}

auto readFrame(BufferedTcpStream &stream) -> ilias::IoTask<proto::Frame> {
    std::array<std::byte, HeaderSize> headerBytes {};
    auto headerRead = co_await stream.readAll(ilias::makeBuffer(headerBytes));
    if (!headerRead) {
        co_return ilias::Err(headerRead.error());
    }

    auto magic = std::bit_cast<uint32_t>(std::array<std::byte, sizeof(uint32_t)> {headerBytes[0], headerBytes[1], headerBytes[2], headerBytes[3]});
    auto version = std::bit_cast<uint16_t>(std::array<std::byte, sizeof(uint16_t)> {headerBytes[4], headerBytes[5]});
    auto payloadSize = std::bit_cast<uint32_t>(std::array<std::byte, sizeof(uint32_t)> {headerBytes[8], headerBytes[9], headerBytes[10], headerBytes[11]});

    if (magic != proto::FrameHeader::Magic) {
        co_return ilias::Err(make_error_code(std::errc::protocol_error));
    }
    if (version != 1) {
        co_return ilias::Err(make_error_code(std::errc::protocol_not_supported));
    }

    std::vector<std::byte> frameBytes;
    frameBytes.reserve(HeaderSize + payloadSize);
    frameBytes.insert(frameBytes.end(), headerBytes.begin(), headerBytes.end());
    frameBytes.resize(HeaderSize + payloadSize);

    if (payloadSize != 0) {
        auto payloadRead = co_await stream.readAll(std::span<std::byte>(frameBytes).subspan(HeaderSize));
        if (!payloadRead) {
            co_return ilias::Err(payloadRead.error());
        }
    }

    auto frame = proto::decodeFrame(frameBytes);
    if (!frame) {
        co_return ilias::Err(make_error_code(std::errc::protocol_error));
    }
    co_return std::move(*frame);
}

auto makeHello(std::string deviceName, uint32_t screenCount, uint32_t version) -> proto::Frame {
    return {.type = proto::MessageType::Hello, .payload = proto::Hello {.version = version, .screenCount = screenCount, .deviceName = std::move(deviceName)}};
}

auto makeHelloAck(uint32_t screenCount, uint32_t version) -> proto::Frame {
    return {.type = proto::MessageType::HelloAck, .payload = proto::HelloAck {.version = version, .screenCount = screenCount}};
}

auto makePing(uint64_t timestampUs) -> proto::Frame {
    return {.type = proto::MessageType::Ping, .payload = proto::Ping {.timestampUs = timestampUs}};
}

auto makePong(uint64_t timestampUs) -> proto::Frame {
    return {.type = proto::MessageType::Pong, .payload = proto::Pong {.timestampUs = timestampUs}};
}

} // namespace mksync::transport


