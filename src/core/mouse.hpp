#pragma once

#include <cstdint>
#include <format>

namespace mksync::core {

/**
 * @brief The mouse button, platform independent
 * 
 */
enum class MouseButton {
    None = 0,
    Left = 1,
    Right,
    Middle,
};

} // namespace mksync::core

// MARK: Formatter
template <>
struct std::formatter<mksync::core::MouseButton> {
    constexpr auto parse(auto &ctxt) { return ctxt.begin(); }

    auto format(const auto &button, auto &ctxt) const {
        using enum mksync::core::MouseButton;
        switch (button) {
            case None: return std::format_to(ctxt.out(), "None");
            case Left: return std::format_to(ctxt.out(), "Left");
            case Right: return std::format_to(ctxt.out(), "Right");
            case Middle: return std::format_to(ctxt.out(), "Middle");
            default: return std::format_to(ctxt.out(), "Unknown");
        }
    }
};