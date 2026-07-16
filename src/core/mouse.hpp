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
FORMATTER(MouseButton);

MKS_END

REFL_REGISTER_FMT_FORMATTER(mks::MouseButton);
