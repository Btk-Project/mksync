#pragma once

#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "../platform/platform.hpp"

namespace mksync::proto {

enum class MessageType : uint16_t {
    Hello        = 0x0001,
    HelloAck     = 0x0002,
    Ping         = 0x0101,
    Pong         = 0x0102,
    ScreenInfo   = 0x0201,
    MouseMove    = 0x0301,
    MousePress   = 0x0302,
    MouseRelease = 0x0303,
    MouseWheel   = 0x0304,
    KeyPress     = 0x0401,
    KeyRelease   = 0x0402,
    Error        = 0xFFFF,
};

struct Hello {
    uint32_t version = 0;
    uint32_t screenCount = 0;
    std::string deviceName;
};

struct HelloAck {
    uint32_t version = 0;
    uint32_t screenCount = 0;
};

struct Ping {
    uint64_t timestampUs = 0;
};

struct Pong {
    uint64_t timestampUs = 0;
};

struct ScreenInfo {
    uint32_t index = 0;
    int32_t x = 0;
    int32_t y = 0;
    int32_t width = 0;
    int32_t height = 0;
    int32_t dpi = 72;
    std::string name;
    bool primary = false;
};

struct MouseMove {
    uint32_t screenIndex = 0;
    int32_t x = 0;
    int32_t y = 0;
};

struct MouseButton {
    uint32_t screenIndex = 0;
    int32_t x = 0;
    int32_t y = 0;
    core::MouseButton button = core::MouseButton::None;
};

struct MouseWheel {
    uint32_t screenIndex = 0;
    int32_t x = 0;
    int32_t y = 0;
    int32_t deltaX = 0;
    int32_t deltaY = 0;
};

struct KeyEvent {
    core::Key key = core::Key::None;
    core::KeyModifier modifiers = core::KeyModifier::None;
    uint32_t nativeCode = 0;
    bool repeat = false;
};

struct Error {
    std::string message;
};

using Message = std::variant<Hello, HelloAck, Ping, Pong, ScreenInfo, MouseMove, MouseButton, MouseWheel, KeyEvent, Error>;

auto messageType(const Message &message) -> MessageType;
auto toMessage(const platform::InputEvent &event) -> std::optional<Message>;
auto toInputEvent(MessageType type, const Message &message) -> std::optional<platform::InputEvent>;

} // namespace mksync::proto

template <>
struct std::formatter<mksync::proto::MessageType> {
    constexpr auto parse(auto &ctxt) const {
        return ctxt.begin();
    }

    auto format(const auto &type, auto &ctxt) const {
        using enum mksync::proto::MessageType;
        switch (type) {
            case Hello: return std::format_to(ctxt.out(), "Hello");
            case HelloAck: return std::format_to(ctxt.out(), "HelloAck");
            case Ping: return std::format_to(ctxt.out(), "Ping");
            case Pong: return std::format_to(ctxt.out(), "Pong");
            case ScreenInfo: return std::format_to(ctxt.out(), "ScreenInfo");
            case MouseMove: return std::format_to(ctxt.out(), "MouseMove");
            case MousePress: return std::format_to(ctxt.out(), "MousePress");
            case MouseRelease: return std::format_to(ctxt.out(), "MouseRelease");
            case MouseWheel: return std::format_to(ctxt.out(), "MouseWheel");
            case KeyPress: return std::format_to(ctxt.out(), "KeyPress");
            case KeyRelease: return std::format_to(ctxt.out(), "KeyRelease");
            case Error: return std::format_to(ctxt.out(), "Error");
            default: return std::format_to(ctxt.out(), "Unknown");
        }
    }
};


