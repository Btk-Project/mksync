#pragma once

#include "preinclude.hpp"
#include <ilias/task.hpp>
#include <ilias/io.hpp>
#include <variant>
#include <format>
#include <system_error>
#include "core.hpp"

MKS_BEGIN

class InputCapture {
public:
    using Ptr = std::shared_ptr<InputCapture>;

    virtual ~InputCapture() = default;

    virtual auto initialize() -> IoTask<void> = 0;
    virtual auto shutdown() -> Task<void> = 0;
    virtual auto nextEvent() -> Task<InputEvent> = 0;
    virtual auto setRemoteControlActive(bool active) -> IoResult<void> {
        (void) active;
        return {};
    }
    virtual auto moveLocalCursor(uint32_t screenIndex, int32_t x, int32_t y) -> IoResult<void> {
        (void) screenIndex;
        (void) x;
        (void) y;
        return Err(std::make_error_code(std::errc::operation_not_supported));
    }
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
