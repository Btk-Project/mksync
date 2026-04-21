#pragma once
#include <mksync/_proto/_config.hpp>
// Standard Library
#include <cstdint>
#include <format>
#include <string>
#include <variant>
#include <vector>
// System Library
// Third-Party Library
// Local Library
#include <mksync/_proto/key.hpp>
#include <mksync/_proto/mouse.hpp>
#include <mksync/_proto/screen.hpp>
#include <mksync/_proto/clipboard.hpp>
// #include "../platform/platform.hpp"

MKS_BEGIN
MKS_PROTO_BEGIN

enum class MessageType : uint32_t
{
    // control messages
    eAck   = 0x00000001, // acknowledge for any message
    eHello = 0x00000002, // client -> server: hello message, used to initiate a connection
    eBye   = 0x00000003, // client <-> server: bye message, used to terminate a connection
    ePing  = 0x00010001, // server -> client: ping message, used to test connectivity
    ePong  = 0x00010002, // client -> server: pong message, used to respond to ping
    // screen topology messages
    eScreenInfo  = 0x00020001,
    eScreenEnter = 0x00020002,
    eScreenLeave = 0x00020003,
    // mouse events messages
    eMouseDown   = 0x00030001,
    eMouseUp     = 0x00030002,
    eMouseRepeat = 0x00030003,
    eMouseMove   = 0x00030004,
    eMouseWheel  = 0x00030005,
    // keyboard events messages
    eKeyboardDown   = 0x00040001,
    eKeyboardUp     = 0x00040002,
    eKeyboardRepeat = 0x00040003,
    // error message
    eError = 0xFFFFFFFF,
};

struct Ack {
    MessageType type;
};

struct Hello {
    std::string name;
};

struct Bye {};

struct Ping {
    uint64_t timestampUs;
};
using Pong = Ping;

struct ScreenDetail {
    uint32_t width;
    uint32_t height;
    int32_t  x;
    int32_t  y;
};

struct ScreenInfo {
    std::vector<ScreenDetail> screens;
};

struct ScreenEnter {
    uint32_t screenIndex;
};

struct ScreenLeave {
    uint32_t screenIndex;
};

struct MouseDown {
    uint32_t    screenIndex = 0;
    MouseButton button      = MouseButton::eNone;
    int32_t     x           = 0;
    int32_t     y           = 0;
};

struct MouseUp {
    uint32_t    screenIndex = 0;
    MouseButton button      = MouseButton::eNone;
    int32_t     x           = 0;
    int32_t     y           = 0;
};

struct MouseRepeat {
    uint32_t    screenIndex = 0;
    MouseButton button      = MouseButton::eNone;
    int32_t     x           = 0;
    int32_t     y           = 0;
};

struct MouseMove {
    uint32_t screenIndex = 0;
    int32_t  x           = 0;
    int32_t  y           = 0;
};

struct MouseWheel {
    uint32_t screenIndex = 0;
    int32_t  x           = 0;
    int32_t  y           = 0;
    int32_t  deltaX      = 0;
    int32_t  deltaY      = 0;
};

struct KeyboardDown {
    KeyModifier modifier;
    KeyLock     lockState;
    KeyCode     code;
};

struct KeyboardUp {
    KeyModifier modifier;
    KeyLock     lockState;
    KeyCode     code;
};

struct KeyboardRepeat {
    KeyModifier modifier;
    KeyLock     lockState;
    KeyCode     code;
};

struct Error {
    uint32_t code;
    uint32_t category;
};

using Message = std::variant<Ack,            //
                             Hello,          //
                             Bye,            //
                             Ping,           //
                             Pong,           //
                             ScreenInfo,     //
                             ScreenEnter,    //
                             ScreenLeave,    //
                             MouseDown,      //
                             MouseUp,        //
                             MouseRepeat,    //
                             MouseMove,      //
                             MouseWheel,     //
                             KeyboardDown,   //
                             KeyboardUp,     //
                             KeyboardRepeat, //
                             Error>;

// auto messageType(const Message &message) -> MessageType;
// auto toMessage(const platform::InputEvent &event) -> std::optional<Message>;
// auto toInputEvent(MessageType type, const Message &message) ->
// std::optional<platform::InputEvent>;

MKS_PROTO_END
MKS_END

template <>
struct std::formatter<mks::proto::MessageType> {
    constexpr auto parse(auto &ctxt) const { return ctxt.begin(); }

    auto format(const auto &type, auto &ctxt) const
    {
        using enum mks::proto::MessageType;
        switch (type) {
            // control messages
        case eAck:
            return std::format_to(ctxt.out(), "Ack");
        case eHello:
            return std::format_to(ctxt.out(), "Hello");
        case eBye:
            return std::format_to(ctxt.out(), "Bye");
        case ePing:
            return std::format_to(ctxt.out(), "Ping");
        case ePong:
            return std::format_to(ctxt.out(), "Pong");
            // screen topology messages
        case eScreenInfo:
            return std::format_to(ctxt.out(), "ScreenInfo");
        case eScreenEnter:
            return std::format_to(ctxt.out(), "ScreenEnter");
        case eScreenLeave:
            return std::format_to(ctxt.out(), "ScreenLeave");
            // mouse events messages
        case eMouseDown:
            return std::format_to(ctxt.out(), "MouseDown");
        case eMouseUp:
            return std::format_to(ctxt.out(), "MouseUp");
        case eMouseRepeat:
            return std::format_to(ctxt.out(), "MouseRepeat");
        case eMouseMove:
            return std::format_to(ctxt.out(), "MouseMove");
        case eMouseWheel:
            return std::format_to(ctxt.out(), "MouseWheel");
            // keyboard events messages
        case eKeyboardDown:
            return std::format_to(ctxt.out(), "KeyboardDown");
        case eKeyboardUp:
            return std::format_to(ctxt.out(), "KeyboardUp");
        case eKeyboardRepeat:
            return std::format_to(ctxt.out(), "KeyboardRepeat");
            // error message
        case eError:
            return std::format_to(ctxt.out(), "Error");
        default:
            return std::format_to(ctxt.out(), "Unknown");
        }
    }
};
