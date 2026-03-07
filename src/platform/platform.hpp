#pragma once

#include <ilias/task.hpp>
#include <ilias/io.hpp>
#include <variant>
#include <memory>
#include <format>

namespace mksync::platform {

// Import types
using ilias::IoTask;
using ilias::Task;
using ilias::Err;

/**
 * @brief The mouse button
 * 
 */
enum class MouseButton {
    None = 0,
    Left = 1,
    Right,
    Middle,
};

/**
 * @brief The raw input event collect from the input capture.
 * 
 */
class InputEvent {
public:
    enum Type {
        None      = 0, // Should never happen
        MouseMove = 1, // The mouse was moved
        MousePress,
        MouseRelease,

        // Keyboard
        KeyPress,
        KeyRelease,

        // Screen
        ScreenChange, // The screen was changed
    } type = None;

    union {
        struct {
            uint32_t    screenIndex; // The screen index (mouse on which screen)
            int32_t     x;
            int32_t     y;
            MouseButton button;
        } mouse;
    };
};

/**
 * @brief The capture interface, capture the mouse and keyboard events.
 * 
 */
class InputCapture {
public:
    using Ptr = std::shared_ptr<InputCapture>;

    virtual ~InputCapture() = default;

    virtual auto initialize() -> IoTask<void> = 0;
    virtual auto shutdown() -> Task<void> = 0;

    /**
     * @brief Get the next event from the input capture.
     * @note It must call the `initialize` method before calling this method.
     * 
     * @return IoTask<InputEvent> 
     */
    virtual auto nextEvent() -> IoTask<InputEvent> = 0;
};

class InputInjector {
public:
    using Ptr = std::shared_ptr<InputInjector>;

    virtual ~InputInjector() = default;

    virtual auto initialize() -> IoTask<void> = 0;
    virtual auto shutdown() -> Task<void> = 0;

    /**
     * @brief Send an event to
     * 
     */
    virtual auto sendEvent(InputEvent event) -> IoTask<void> = 0;
};

/**
 * @brief Create an platform specific input capture
 * 
 * @return InputCapture::Ptr 
 */
auto createInputCapture() -> InputCapture::Ptr;

} // namespace mksync::platform

// Formatter
template <>
struct std::formatter<mksync::platform::MouseButton> {
    constexpr auto parse(auto &ctxt) { return ctxt.begin(); }

    auto format(const auto &button, auto &ctxt) const {
        using enum mksync::platform::MouseButton;
        switch (button) {
            case None: return std::format_to(ctxt.out(), "None");
            case Left: return std::format_to(ctxt.out(), "Left");
            case Right: return std::format_to(ctxt.out(), "Right");
            case Middle: return std::format_to(ctxt.out(), "Middle");
            default: return std::format_to(ctxt.out(), "Unknown");
        }
    }
};