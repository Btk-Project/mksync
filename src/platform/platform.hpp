#pragma once

#include "preinclude.hpp"
#include <ilias/task.hpp>
#include <ilias/io.hpp>
#include <variant>
#include "core.hpp"

MKS_BEGIN

struct InputEvent : std::variant<
    KeyEvent,
    MouseButtonEvent,
    MouseMoveEvent,
    MouseWheelEvent
> {};

class InputCapture {
public:
    using Ptr = std::shared_ptr<InputCapture>;

    virtual ~InputCapture() = default;

    virtual auto initialize() -> IoTask<void> = 0;
    virtual auto shutdown() -> Task<void> = 0;
    virtual auto nextEvent() -> Task<InputEvent> = 0;
};

class InputInjector {
public:
    using Ptr = std::shared_ptr<InputInjector>;

    virtual ~InputInjector() = default;

    virtual auto initialize() -> IoTask<void> = 0;
    virtual auto shutdown() -> Task<void> = 0;
};

class Platform {
public:
    using Ptr = std::shared_ptr<Platform>;

    virtual ~Platform() = default;

    virtual auto screens() const -> std::vector<ScreenInfo> = 0;
    virtual auto createCapture() -> InputCapture::Ptr = 0;
    virtual auto createInjector() -> InputInjector::Ptr = 0;

    /**
     * @brief Create current compiled platform
     * 
     * @return Platform::Ptr 
     */
    static auto create() -> Ptr;
};

MKS_END