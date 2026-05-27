#pragma once

#include "preinclude.hpp"
#include <ilias/task.hpp>
#include <ilias/io.hpp>
#include <variant>
#include <format>
#include "core.hpp"

MKS_BEGIN

/**
 * @brief The event used by capture & injector
 * 
 */
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
    virtual auto inject(const InputEvent &event) -> IoTask<void> = 0;
};

/**
 * @brief The virtual platform class
 * 
 */
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

// Formatter for InputEvent
template <>
struct std::formatter<mks::InputEvent> {
    constexpr auto parse(std::format_parse_context &ctxt) -> decltype(ctxt.begin()) {
        return ctxt.begin();
    }

    template <typename FormatContext>
    auto format(const mks::InputEvent &event, FormatContext &ctxt) const -> decltype(ctxt.out()) {
        return event.visit([&](const auto &e) {
            return std::format_to(ctxt.out(), "{}", e);
        });
    }
};