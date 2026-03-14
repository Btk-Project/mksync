#pragma once

#include <ilias/task.hpp>
#include <ilias/io.hpp>
#include <cstdint>
#include <format>
#include <memory>
#include <span>
#include <string>
#include <system_error>
#include <variant>
#include <vector>

#include "../core/mouse.hpp"
#include "../core/key.hpp"

namespace mksync::platform {

using ilias::Err;
using ilias::IoTask;
using ilias::Task;

using core::Key;
using core::KeyModifier;
using core::MouseButton;

class InputEvent {
public:
    enum class Type {
        None = 0,

        // Mouse
        MouseMove,
        MousePress,
        MouseRelease,
        MouseWheel,

        // Keyboard
        KeyPress,
        KeyRelease,

        // Screen / topology
        ScreenChange,
    };

    struct Metadata {
        uint64_t timestampUs = 0;
        uint32_t screenIndex = 0;
        bool injected = false;
    };

    struct MouseMoveData {
        int32_t x = 0;
        int32_t y = 0;
    };

    struct MouseButtonData {
        int32_t x = 0;
        int32_t y = 0;
        MouseButton button = MouseButton::None;
    };

    struct MouseWheelData {
        int32_t x = 0;
        int32_t y = 0;
        int32_t deltaX = 0;
        int32_t deltaY = 0;
    };

    struct KeyData {
        Key key = Key::None;
        KeyModifier modifiers = KeyModifier::None;
        uint32_t nativeCode = 0;
        bool repeat = false;
    };

    struct ScreenChangeData {
        uint32_t screenCount = 0;
    };

    using Payload = std::variant<
        std::monostate,
        MouseMoveData,
        MouseButtonData,
        MouseWheelData,
        KeyData,
        ScreenChangeData
    >;

    Type type = Type::None;
    Metadata metadata {};
    Payload payload {};

    template <typename T>
    auto getIf() -> T * {
        return std::get_if<T>(&payload);
    }

    template <typename T>
    auto getIf() const -> const T * {
        return std::get_if<T>(&payload);
    }
};

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

class InputCapture {
public:
    using Ptr = std::shared_ptr<InputCapture>;

    virtual ~InputCapture() = default;

    virtual auto initialize() -> IoTask<void> = 0;
    virtual auto shutdown() -> Task<void> = 0;
    virtual auto nextEvent() -> IoTask<InputEvent> = 0;
};

class InputInjector {
public:
    using Ptr = std::shared_ptr<InputInjector>;

    virtual ~InputInjector() = default;

    virtual auto initialize() -> IoTask<void> = 0;
    virtual auto shutdown() -> Task<void> = 0;
    virtual auto sendEvents(std::span<const InputEvent> events) -> IoTask<void> = 0;
    virtual auto sendEventsSync(std::span<const InputEvent> events) -> std::error_code = 0;
};

class Platform {
public:
    using Ptr = std::shared_ptr<Platform>;

    virtual ~Platform() = default;

    virtual auto screens() const -> std::vector<ScreenInfo> = 0;

    virtual auto createInputCapture() -> InputCapture::Ptr = 0;
    virtual auto createInputInjector() -> InputInjector::Ptr = 0;
};

auto createPlatform() -> Platform::Ptr;

} // namespace mksync::platform

template <>
struct std::formatter<mksync::platform::InputEvent::Type> {
    constexpr auto parse(auto &ctxt) {
        return ctxt.begin();
    }

    auto format(const auto &type, auto &ctxt) const {
        using enum mksync::platform::InputEvent::Type;
        switch (type) {
            case None: return std::format_to(ctxt.out(), "None");
            case MouseMove: return std::format_to(ctxt.out(), "MouseMove");
            case MousePress: return std::format_to(ctxt.out(), "MousePress");
            case MouseRelease: return std::format_to(ctxt.out(), "MouseRelease");
            case MouseWheel: return std::format_to(ctxt.out(), "MouseWheel");
            case KeyPress: return std::format_to(ctxt.out(), "KeyPress");
            case KeyRelease: return std::format_to(ctxt.out(), "KeyRelease");
            case ScreenChange: return std::format_to(ctxt.out(), "ScreenChange");
            default: return std::format_to(ctxt.out(), "Unknown");
        }
    }
};

template <>
struct std::formatter<mksync::platform::InputEvent> {
    constexpr auto parse(auto &ctxt) {
        return ctxt.begin();
    }

    auto format(const auto &event, auto &ctxt) const {
        using Event = mksync::platform::InputEvent;
        using enum Event::Type;

        auto prefix = std::format(
            "InputEvent(type: {}, screen: {}, injected: {}, ts_us: {}",
            event.type,
            event.metadata.screenIndex,
            event.metadata.injected,
            event.metadata.timestampUs
        );

        switch (event.type) {
            case MouseMove: {
                if (auto data = event.getIf<Event::MouseMoveData>()) {
                    return std::format_to(ctxt.out(), "{}, x: {}, y: {})", prefix, data->x, data->y);
                }
                break;
            }
            case MousePress:
            case MouseRelease: {
                if (auto data = event.getIf<Event::MouseButtonData>()) {
                    return std::format_to(ctxt.out(), "{}, button: {}, x: {}, y: {})", prefix, data->button, data->x, data->y);
                }
                break;
            }
            case MouseWheel: {
                if (auto data = event.getIf<Event::MouseWheelData>()) {
                    return std::format_to(ctxt.out(), "{}, x: {}, y: {}, deltaX: {}, deltaY: {})", prefix, data->x, data->y, data->deltaX, data->deltaY);
                }
                break;
            }
            case KeyPress:
            case KeyRelease: {
                if (auto data = event.getIf<Event::KeyData>()) {
                    return std::format_to(
                        ctxt.out(),
                        "{}, key: 0x{:X}, modifiers: 0x{:X}, native: 0x{:X}, repeat: {})",
                        prefix,
                        static_cast<uint32_t>(data->key),
                        static_cast<uint32_t>(data->modifiers),
                        data->nativeCode,
                        data->repeat
                    );
                }
                break;
            }
            case ScreenChange: {
                if (auto data = event.getIf<Event::ScreenChangeData>()) {
                    return std::format_to(ctxt.out(), "{}, screenCount: {})", prefix, data->screenCount);
                }
                break;
            }
            case None:
                return std::format_to(ctxt.out(), "{})", prefix);
        }
        return std::format_to(ctxt.out(), "{}, payload: <invalid>)", prefix);
    }
};
