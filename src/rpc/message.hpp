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
#include "refl/serde.hpp"
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

    Ping,
    Pong,

    Error = 0xFFFF
};
FORMATTER(MessageId);

/**
 * @brief Hello message, the first message sent by client to server
 * 
 */
struct HelloMessage {
    static constexpr auto Id = MessageId::Hello;
    uint16_t    version = 0;
    std::string name;
};
FORMATTER(HelloMessage);

/**
 * @brief The message sent by client to server when screens are changed
 * 
 */
struct ScreensMessage {
    static constexpr auto Id = MessageId::Screens;
    std::vector<ScreenInfo> screens;
};
FORMATTER(ScreensMessage);

/**
 * @brief The message server <-> client when an error occurs
 * 
 */
struct ErrorMessage {
    static constexpr auto Id = MessageId::Error;
    std::string message;
};
FORMATTER(ErrorMessage);

/**
 * @brief The message sent <-> received by client and server
 * 
 */
struct PingMessage {
    static constexpr auto Id = MessageId::Ping;
    uint32_t pad = 0;
};
struct PongMessage {
    static constexpr auto Id = MessageId::Pong;
    uint32_t pad = 0;
};
FORMATTER(PingMessage);
FORMATTER(PongMessage);


template<typename... Ts>
struct VariantBase : std::variant<Ts...> {
    using Base = std::variant<Ts...>;
    using Base::Base;
};

/**
 * @brief The message transmitted between client and server
 * 
 */
struct RpcMessage : VariantBase<
    HelloMessage,
    ScreensMessage,
    PingMessage,
    PongMessage,
    ErrorMessage
> {};
VARIANT_FORMATTER(RpcMessage);

// TODO: 
template <typename T>
concept MessageLike = requires(T t, Serializer sr, Deserializer ds) {
    { sr(t) } -> std::same_as<bool>;
    { ds(t) } -> std::same_as<bool>;
    { T::Id } -> std::same_as<MessageId>;
};

MKS_END