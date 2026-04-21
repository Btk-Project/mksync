#pragma once
#include <mksync/_proto/_config.hpp>
// Standard Library
#include <cstdint>
#include <format>
// System Library
// Third-Party Library
// Local Library

MKS_BEGIN
MKS_PROTO_BEGIN

/**
 * @brief The mouse button, platform independent
 *
 */
enum class MouseButton : uint8_t
{
    eNone   = 0,
    eLeft   = 1,
    eRight  = 2,
    eMiddle = 3,
    eExtra0 = 4,
    eExtra1 = 5,
    eExtra2 = 6,
};

MKS_PROTO_END
MKS_END

// MARK: Formatter
template <>
struct std::formatter<mks::proto::MouseButton> {
    constexpr auto parse(auto &ctxt) { return ctxt.begin(); }

    auto format(const auto &button, auto &ctxt) const
    {
        using enum mks::proto::MouseButton;
        switch (button) {
        case eNone:
            return std::format_to(ctxt.out(), "None");
        case eLeft:
            return std::format_to(ctxt.out(), "Left");
        case eRight:
            return std::format_to(ctxt.out(), "Right");
        case eMiddle:
            return std::format_to(ctxt.out(), "Middle");
        default:
            return std::format_to(ctxt.out(), "Unknown");
        }
    }
};
