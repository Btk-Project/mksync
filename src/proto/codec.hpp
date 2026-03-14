#pragma once

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

#include "message.hpp"

namespace mksync::proto {

struct Frame {
    MessageType type = MessageType::Error;
    Message payload = Error {"empty"};
};

struct FrameHeader {
    static constexpr uint32_t Magic = 0x4D4B5331; // MKS1

    uint32_t magic = Magic;
    uint16_t version = 1;
    MessageType type = MessageType::Error;
    uint32_t payloadSize = 0;
};

auto encodeFrame(const Frame &frame) -> std::vector<std::byte>;
auto decodeFrame(std::span<const std::byte> bytes) -> std::expected<Frame, std::string>;

} // namespace mksync::proto
