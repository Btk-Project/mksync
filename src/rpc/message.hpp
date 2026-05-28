/**
 * @file message.hpp
 * @author BusyStudent (fyw90mc@gmail.com)
 * @brief Used for Client <-> Server communication
 * @version 0.1
 * @date 2026-05-27
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#pragma once

#include "preinclude.hpp"
#include "refl/formatter.hpp"
#include "core.hpp"
#include <variant>
#include <format>

MKS_BEGIN

/**
 * @brief Message ID
 * 
 */
enum class MessageId : uint16_t {
    Hello = 0,
    Screens,

    Error = 0xFFFF
};

/**
 * @brief Hello message, the first message sent by client to server
 * 
 */
struct HelloMessage {
    uint16_t    version = 0;
    std::string name;
};
FORMATTER(HelloMessage);

/**
 * @brief The message sent by client to server when screens are changed
 * 
 */
struct ScreensMessage {
    std::vector<ScreenInfo> screens;
};
FORMATTER(ScreensMessage);

/**
 * @brief The message server <-> client when an error occurs
 * 
 */
struct ErrorMessage {
    std::string message;
};
FORMATTER(ErrorMessage);

/**
 * @brief The message transmitted between client and server
 * 
 */
struct RpcMessage : std::variant<
    HelloMessage,
    ScreensMessage,
    ErrorMessage
> {
    auto toBytes(std::vector<std::byte> &buf) -> size_t;
};


template <typename T>
concept MessageLike = requires(T t) {
    { t.toBytes(std::declval<std::vector<std::byte> &>()) } -> std::same_as<void>;
    { t.fromBytes() } -> std::same_as<IoResult<T> >;
    { T::Id } -> std::same_as<MessageId>;
};

MKS_END

// Formatter for RpcMessage
template <>
struct std::formatter<mks::RpcMessage> {
    constexpr auto parse(std::format_parse_context &ctxt) -> decltype(ctxt.begin()) {
        return ctxt.begin();
    }

    template <typename FormatContext>
    auto format(const mks::RpcMessage &message, FormatContext &ctxt) const -> decltype(ctxt.out()) {
        return message.visit([&](const auto &e) {
            return std::format_to(ctxt.out(), "{}", e);
        });
    }
};