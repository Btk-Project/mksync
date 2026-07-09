#pragma once

#include "preinclude.hpp"
#include "refl/formatter.hpp"
#include "mouse.hpp"
#include "key.hpp"
#include <variant>
#include <vector>

MKS_BEGIN

// Input events
struct MouseMoveEvent {
    int32_t x = 0;            // X in pixels
    int32_t y = 0;            // Y in pixels
    uint32_t screenIndex = 0; // Index of the screen
    int32_t deltaX = 0;       // Relative X movement when the backend provides raw motion
    int32_t deltaY = 0;       // Relative Y movement when the backend provides raw motion
};
FORMATTER(MouseMoveEvent);

struct MouseButtonEvent {
    int32_t x = 0;            // X in pixels
    int32_t y = 0;            // Y in pixels
    uint32_t screenIndex = 0; // Index of the screen
    MouseButton button = MouseButton::None; // Button that was pressed
    bool        release = false;            // Is the button being released
};
FORMATTER(MouseButtonEvent);

struct MouseWheelEvent {
    int32_t x = 0;      // X in pixels
    int32_t y = 0;      // Y in pixels
    int32_t deltaX = 0;
    int32_t deltaY = 0;
};
FORMATTER(MouseWheelEvent);

struct KeyEvent {
    Key         key = Key::None;               // Key that was pressed
    KeyModifier modifiers = KeyModifier::None; // Modifier keys that were pressed
    uint32_t    nativeCode = 0;                // Native code of the key
    bool        repeat = false;                // Is the key being held down
    bool        release = false;               // Is the key being released
};
FORMATTER(KeyEvent);

// Screen Events
struct ScreenInfo {
    int32_t x = 0;
    int32_t y = 0;
    int32_t width = 0;
    int32_t height = 0;
    int32_t dpi = 72;
    std::string name;
    bool primary = false;
};
FORMATTER(ScreenInfo);

struct ScreenChangeEvent {
    std::vector<ScreenInfo> screens;
};
FORMATTER(ScreenChangeEvent);

/**
 * @brief The event used by capture & injector
 * 
 */
using InputEvent = std::variant<
    KeyEvent,
    MouseButtonEvent,
    MouseMoveEvent,
    MouseWheelEvent
>;
VARIANT_FORMATTER(InputEvent);

MKS_END
