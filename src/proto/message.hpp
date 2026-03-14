#pragma once

#include <cstdint>
#include <format>
#include <array>
#include <span>
#include <bit>

namespace mksync::proto {

// Network Message format
// u16 type | u16 len | u8[0..len-2] |
enum class MessageType : uint16_t {
    // Handshake
    Hello        = 0x0001, // Client -> Server
    HelloAck     = 0x0002, // Server -> Client

    // Heartbeat
    Ping         = 0x0101,
    Pong         = 0x0102,

    // Screen
    ScreenInfo   = 0x0201,

    // Mouse
    MouseMove    = 0x0301,
    MousePress   = 0x0302,
    MouseRelease = 0x0303,

    // Keyboard
    KeyPress     = 0x0401,
    KeyRelease   = 0x0402,

    // Error
    Error        = 0xFFFF,
};

class Hello {
public:
    uint32_t    version = 0;
    std::string deviceName;
};

class HelloAck {
public:
    uint32_t    version = 0;
};

class Ping {
public:
    
};

class Pong {
public:

};

class Error {
public:
    std::string message;
};

} // namespace mksync::proto


// Formatter
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
            case KeyPress: return std::format_to(ctxt.out(), "KeyPress");
            case KeyRelease: return std::format_to(ctxt.out(), "KeyRelease");
            case Error: return std::format_to(ctxt.out(), "Error");
            default: return std::format_to(ctxt.out(), "Unknown");
        }
    }
};