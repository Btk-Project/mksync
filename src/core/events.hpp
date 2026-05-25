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
};
STRUCT_FORMATTER(MouseMoveEvent);

struct MouseButtonEvent {
    int32_t x = 0;            // X in pixels
    int32_t y = 0;            // Y in pixels
    uint32_t screenIndex = 0; // Index of the screen
    MouseButton button = MouseButton::None; // Button that was pressed
    bool        release = false;            // Is the button being released
};
STRUCT_FORMATTER(MouseButtonEvent);

struct MouseWheelEvent {
    int32_t x = 0;      // X in pixels
    int32_t y = 0;      // Y in pixels
    int32_t deltaX = 0;
    int32_t deltaY = 0;
};
STRUCT_FORMATTER(MouseWheelEvent);

struct KeyEvent {
    Key         key = Key::None;               // Key that was pressed
    KeyModifier modifiers = KeyModifier::None; // Modifier keys that were pressed
    uint32_t    nativeCode = 0;                // Native code of the key
    bool        repeat = false;                // Is the key being held down
    bool        release = false;               // Is the key being released
};
STRUCT_FORMATTER(KeyEvent);

// Screen Events
class ScreenInfo {
public:
    int32_t x = 0;
    int32_t y = 0;
    int32_t width = 0;
    int32_t height = 0;
    int32_t dpi = 72;
    std::string name;
    bool primary = false;
};
STRUCT_FORMATTER(ScreenInfo);

struct ScreenChangeEvent {
    std::vector<ScreenInfo> screens;
};
STRUCT_FORMATTER(ScreenChangeEvent);

MKS_END
