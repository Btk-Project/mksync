#pragma once
#include <mksync/_proto/_config.hpp>
// Standard Library
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>
// System Library
// Third-Party Library
// Local Library
#include <mksync/_proto/message.hpp>

MKS_BEGIN
MKS_PROTO_BEGIN

struct FrameHeader {
    static constexpr uint32_t Magic = 0x4D4B5331; // MKS1

    uint32_t    magic       = Magic;
    uint32_t    version     = 1;
    MessageType type        = MessageType::eError;
    uint32_t    payloadSize = 0;
};

auto encodeFrame(const Message &msg) -> std::vector<std::byte>;
auto decodeFrame(std::span<const std::byte> bytes) -> std::expected<Message, std::error_code>;

MKS_PROTO_END
MKS_END
