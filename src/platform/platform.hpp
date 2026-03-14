#pragma once

#include <ilias/task.hpp>
#include <ilias/io.hpp>
#include <variant>
#include <memory>
#include <format>
#include <span>

#include "../core/mouse.hpp"
#include "../core/key.hpp"

namespace mksync::platform {

// Import types
using ilias::IoTask;
using ilias::Task;
using ilias::Err;

using core::MouseButton;
using core::KeyModifier;
using core::Key;

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
        ScreenChange, // The screen was changed, Not supported in injector
    } type = None;

    union {
        struct {
            // Common fields for mouse events
            uint32_t    screenIndex; // The screen index (mouse on which screen)
            int32_t     x;
            int32_t     y;

            // Mouse press/release
            MouseButton button;
        } mouse;

        struct {
            Key         key;
            KeyModifier modifiers;
        } key;
    };
};

class ScreenInfo {
public:
    int32_t width = 0;
    int32_t height = 0;
    int32_t dpi = 72;
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
     * @brief Send an event to injector.
     * @note Only some events are supported, see `InputEvent::Type`.
     * 
     */
    virtual auto sendEvents(std::span<const InputEvent> events) -> IoTask<void> = 0;
};

/**
 * @brief The platform interface, used to manage the screen & other platform specific things.
 * 
 */
class Platform {
public:
    using Ptr = std::shared_ptr<Platform>;

    virtual ~Platform() = default;

    /**
     * @brief Get the current all screens info.
     * 
     * @return std::vector<ScreenInfo> 
     */
    // virtual auto screens() -> std::vector<ScreenInfo> = 0;

    // Factory methods
    virtual auto createInputCapture() -> InputCapture::Ptr = 0;
    virtual auto createInputInjector() -> InputInjector::Ptr = 0;
};

/**
 * @brief Create a current platform instance.
 * 
 * @return Platform::Ptr 
 */
auto createPlatform() -> Platform::Ptr;

} // namespace mksync::platform

template <>
struct std::formatter<mksync::platform::InputEvent::Type> {
    constexpr auto parse(auto &ctxt) { return ctxt.begin(); }

    auto format(const auto &type, auto &ctxt) const {
        using enum mksync::platform::InputEvent::Type;
        switch (type) {
            case None: return std::format_to(ctxt.out(), "None");
            case MouseMove: return std::format_to(ctxt.out(), "MouseMove");
            case MousePress: return std::format_to(ctxt.out(), "MousePress");
            case MouseRelease: return std::format_to(ctxt.out(), "MouseRelease");
            case KeyPress: return std::format_to(ctxt.out(), "KeyPress");
            default: return std::format_to(ctxt.out(), "Unknown");
        }
    }
};

template <>
struct std::formatter<mksync::platform::InputEvent> {
    constexpr auto parse(auto &ctxt) { return ctxt.begin(); }

    auto format(const auto &event, auto &ctxt) const {
        using enum mksync::platform::InputEvent::Type;
        switch (event.type) {
            case None: return std::format_to(ctxt.out(), "InputEvent(None)");
            case MouseMove: return std::format_to(ctxt.out(), "InputEvent(MouseMove(x: {}, y: {})", event.mouse.x, event.mouse.y);
            case MousePress: return std::format_to(ctxt.out(), "InputEvent(MousePress(button: {}, x: {}, y: {})", event.mouse.button, event.mouse.x, event.mouse.y);
            case MouseRelease: return std::format_to(ctxt.out(), "InputEvent(MouseRelease(button: {}, x: {}, y: {})", event.mouse.button, event.mouse.x, event.mouse.y);
            default: return std::format_to(ctxt.out(), "InputEvent({})", event.type);
        }
    }
};