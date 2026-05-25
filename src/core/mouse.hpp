#pragma once

#include "preinclude.hpp"
#include "refl/formatter.hpp"
#include <type_traits>
#include <cstdint>
#include <format>

MKS_BEGIN

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
ENUM_FORMATTER(MouseButton);

MKS_END