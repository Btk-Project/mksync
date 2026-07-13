#pragma once

#include "platform/platform.hpp"
#include <chrono>
#include <cmath>
#include <ilias/sync.hpp>
#include <mutex>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace mks::test
{

    struct MockCursorMove {
        uint32_t screenIndex = 0;
        int32_t  x           = 0;
        int32_t  y           = 0;
    };

    struct MockDelay {
        std::chrono::nanoseconds duration{};
    };

    struct EventFrequency {
        uint32_t hertz = 0;
    };

    inline constexpr auto kDefaultInputFrequency = EventFrequency{.hertz = 125};

    namespace literals
    {

        constexpr auto operator""_hz(unsigned long long hertz) -> EventFrequency
        {
            return {.hertz = static_cast<uint32_t>(hertz)};
        }

    } // namespace literals

    using MockAction = std::variant<InputEvent, MockDelay>;

    namespace detail
    {

        struct MockPointerState {
            int32_t  x           = 0;
            int32_t  y           = 0;
            uint32_t screenIndex = 0;
        };

        inline auto interpolationStepCount(std::chrono::nanoseconds duration,
                                           EventFrequency           frequency) -> size_t
        {
            if (frequency.hertz == 0) {
                throw std::invalid_argument("input event frequency must be greater than zero");
            }
            if (duration <= std::chrono::nanoseconds::zero()) {
                return 1;
            }
            constexpr auto nanosecondsPerSecond = std::chrono::seconds{1}.count() * 1'000'000'000LL;
            auto           scaled               = duration.count() * frequency.hertz;
            return static_cast<size_t>((scaled + nanosecondsPerSecond - 1) / nanosecondsPerSecond);
        }

        inline auto eventInterval(EventFrequency frequency) -> std::chrono::nanoseconds
        {
            if (frequency.hertz == 0) {
                throw std::invalid_argument("input event frequency must be greater than zero");
            }
            return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds{1}) /
                   frequency.hertz;
        }

        inline auto interpolate(int32_t from, int32_t to, size_t step, size_t stepCount) -> int32_t
        {
            auto ratio = static_cast<double>(step) / static_cast<double>(stepCount);
            return static_cast<int32_t>(std::lround(from + (to - from) * ratio));
        }

        inline auto sameEvent(const InputEvent &lhs, const InputEvent &rhs) -> bool
        {
            if (lhs.index() != rhs.index()) {
                return false;
            }
            return std::visit(Overloads{
                                  [](const MouseMoveEvent &a, const MouseMoveEvent &b) {
                                      return a.x == b.x && a.y == b.y &&
                                             a.screenIndex == b.screenIndex &&
                                             a.deltaX == b.deltaX && a.deltaY == b.deltaY;
                                  },
                                  [](const MouseButtonEvent &a, const MouseButtonEvent &b) {
                                      return a.x == b.x && a.y == b.y &&
                                             a.screenIndex == b.screenIndex &&
                                             a.button == b.button && a.release == b.release;
                                  },
                                  [](const MouseWheelEvent &a, const MouseWheelEvent &b) {
                                      return a.x == b.x && a.y == b.y && a.deltaX == b.deltaX &&
                                             a.deltaY == b.deltaY;
                                  },
                                  [](const KeyEvent &a, const KeyEvent &b) {
                                      return a.key == b.key && a.modifiers == b.modifiers &&
                                             a.nativeCode == b.nativeCode && a.repeat == b.repeat &&
                                             a.release == b.release;
                                  },
                                  [](const auto &, const auto &) { return false; },
                              },
                              lhs, rhs);
        }

    } // namespace detail

    class MockActions {
    public:
        auto frequency(EventFrequency frequency) -> MockActions &
        {
            (void)detail::eventInterval(frequency);
            mFrequency = frequency;
            return *this;
        }

        auto mouse() -> MockActions & { return *this; }

        auto keyboard() -> MockActions & { return *this; }

        auto set(int32_t x, int32_t y, uint32_t screenIndex = 0) -> MockActions &
        {
            mPointer = {.x = x, .y = y, .screenIndex = screenIndex};
            addMouseMove(0, 0);
            return *this;
        }

        template <typename Rep, typename Period>
        auto sleep(std::chrono::duration<Rep, Period> duration) -> MockActions &
        {
            mActions.emplace_back(MockDelay{
                .duration = std::chrono::duration_cast<std::chrono::nanoseconds>(duration),
            });
            return *this;
        }

        template <typename Rep, typename Period>
        auto interpolationMove(int32_t x, int32_t y, std::chrono::duration<Rep, Period> duration,
                               EventFrequency frequency) -> MockActions &
        {
            auto total     = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            auto stepCount = detail::interpolationStepCount(total, frequency);
            auto delay     = total / stepCount;
            auto start     = mPointer;
            for (auto step = 1U; step <= stepCount; ++step) {
                auto nextX = detail::interpolate(start.x, x, step, stepCount);
                auto nextY = detail::interpolate(start.y, y, step, stepCount);
                mActions.emplace_back(MockDelay{.duration = delay});
                auto deltaX = nextX - mPointer.x;
                auto deltaY = nextY - mPointer.y;
                mPointer.x  = nextX;
                mPointer.y  = nextY;
                addMouseMove(deltaX, deltaY);
            }
            return *this;
        }

        auto moveBy(int32_t deltaX, int32_t deltaY) -> MockActions &
        {
            mPointer.x += deltaX;
            mPointer.y += deltaY;
            addMouseMove(deltaX, deltaY);
            return *this;
        }

        auto scroll(int32_t deltaX, int32_t deltaY) -> MockActions &
        {
            mActions.emplace_back(InputEvent{
                MouseWheelEvent{
                                .x      = mPointer.x,
                                .y      = mPointer.y,
                                .deltaX = deltaX,
                                .deltaY = deltaY,
                                }
            });
            return *this;
        }

        auto press(MouseButton button) -> MockActions &
        {
            addMouseButton(button, false);
            return *this;
        }

        auto release(MouseButton button) -> MockActions &
        {
            addMouseButton(button, true);
            return *this;
        }

        auto click(MouseButton button) -> MockActions & { return press(button).release(button); }

        template <typename Rep, typename Period>
        auto click(MouseButton button, std::chrono::duration<Rep, Period> holdDuration)
            -> MockActions &
        {
            return press(button).sleep(holdDuration).release(button);
        }

        template <typename Rep, typename Period>
        auto doubleClick(MouseButton button, std::chrono::duration<Rep, Period> interval)
            -> MockActions &
        {
            return click(button).sleep(interval).click(button);
        }

        auto press(Key key, KeyModifier modifiers = KeyModifier::None, uint32_t nativeCode = 0)
            -> MockActions &
        {
            addKey(key, modifiers, nativeCode, false);
            return *this;
        }

        auto release(Key key, KeyModifier modifiers = KeyModifier::None, uint32_t nativeCode = 0)
            -> MockActions &
        {
            addKey(key, modifiers, nativeCode, true);
            return *this;
        }

        auto click(Key key, KeyModifier modifiers = KeyModifier::None, uint32_t nativeCode = 0)
            -> MockActions &
        {
            return press(key, modifiers, nativeCode).release(key, modifiers, nativeCode);
        }

        template <typename Rep, typename Period>
        auto click(Key key, std::chrono::duration<Rep, Period> holdDuration,
                   KeyModifier modifiers = KeyModifier::None, uint32_t nativeCode = 0)
            -> MockActions &
        {
            return press(key, modifiers, nativeCode)
                .sleep(holdDuration)
                .release(key, modifiers, nativeCode);
        }

        auto chord(KeyModifier modifiers, Key key, uint32_t nativeCode = 0) -> MockActions &
        {
            return click(key, modifiers, nativeCode);
        }

        template <typename Rep, typename Period>
        auto repeat(Key key, std::chrono::duration<Rep, Period> duration, EventFrequency frequency,
                    KeyModifier modifiers = KeyModifier::None, uint32_t nativeCode = 0)
            -> MockActions &
        {
            auto total       = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            auto repeatCount = detail::interpolationStepCount(total, frequency);
            auto interval    = total / repeatCount;
            press(key, modifiers, nativeCode);
            for (auto index = 0U; index < repeatCount; ++index) {
                mActions.emplace_back(MockDelay{.duration = interval});
                addKey(key, modifiers, nativeCode, false, true);
            }
            release(key, modifiers, nativeCode);
            return *this;
        }

        auto steps() const -> const std::vector<MockAction> & { return mActions; }

        auto eventFrequency() const -> EventFrequency { return mFrequency; }

        auto clear() -> void
        {
            mActions.clear();
            mPointer   = {};
            mFrequency = kDefaultInputFrequency;
        }

    private:
        auto addMouseMove(int32_t deltaX, int32_t deltaY) -> void
        {
            mActions.emplace_back(InputEvent{
                MouseMoveEvent{
                               .x           = mPointer.x,
                               .y           = mPointer.y,
                               .screenIndex = mPointer.screenIndex,
                               .deltaX      = deltaX,
                               .deltaY      = deltaY,
                               }
            });
        }

        auto addMouseButton(MouseButton button, bool release) -> void
        {
            mActions.emplace_back(InputEvent{
                MouseButtonEvent{
                                 .x           = mPointer.x,
                                 .y           = mPointer.y,
                                 .screenIndex = mPointer.screenIndex,
                                 .button      = button,
                                 .release     = release,
                                 }
            });
        }

        auto addKey(Key key, KeyModifier modifiers, uint32_t nativeCode, bool release,
                    bool repeat = false) -> void
        {
            mActions.emplace_back(InputEvent{
                KeyEvent{
                         .key        = key,
                         .modifiers  = modifiers,
                         .nativeCode = nativeCode,
                         .repeat     = repeat,
                         .release    = release,
                         }
            });
        }

        detail::MockPointerState mPointer;
        std::vector<MockAction>  mActions;
        EventFrequency           mFrequency = kDefaultInputFrequency;
    };

    class MockExpectations {
    public:
        auto mouse() -> MockExpectations & { return *this; }

        auto keyboard() -> MockExpectations & { return *this; }

        auto set(int32_t x, int32_t y, uint32_t screenIndex = 0) -> MockExpectations &
        {
            mPointer = {.x = x, .y = y, .screenIndex = screenIndex};
            addMouseMove();
            return *this;
        }

        template <typename Rep, typename Period>
        auto interpolationMove(int32_t x, int32_t y, std::chrono::duration<Rep, Period> duration,
                               EventFrequency frequency) -> MockExpectations &
        {
            auto total     = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            auto stepCount = detail::interpolationStepCount(total, frequency);
            auto start     = mPointer;
            for (auto step = 1U; step <= stepCount; ++step) {
                mPointer.x = detail::interpolate(start.x, x, step, stepCount);
                mPointer.y = detail::interpolate(start.y, y, step, stepCount);
                addMouseMove();
            }
            return *this;
        }

        auto moveBy(int32_t deltaX, int32_t deltaY) -> MockExpectations &
        {
            mPointer.x += deltaX;
            mPointer.y += deltaY;
            addMouseMove();
            return *this;
        }

        auto scroll(int32_t deltaX, int32_t deltaY) -> MockExpectations &
        {
            mEvents.emplace_back(MouseWheelEvent{
                .x      = mPointer.x,
                .y      = mPointer.y,
                .deltaX = deltaX,
                .deltaY = deltaY,
            });
            return *this;
        }

        auto press(MouseButton button) -> MockExpectations &
        {
            addMouseButton(button, false);
            return *this;
        }

        auto release(MouseButton button) -> MockExpectations &
        {
            addMouseButton(button, true);
            return *this;
        }

        auto click(MouseButton button) -> MockExpectations &
        {
            return press(button).release(button);
        }

        auto doubleClick(MouseButton button) -> MockExpectations &
        {
            return click(button).click(button);
        }

        auto press(Key key, KeyModifier modifiers = KeyModifier::None, uint32_t nativeCode = 0)
            -> MockExpectations &
        {
            addKey(key, modifiers, nativeCode, false);
            return *this;
        }

        auto release(Key key, KeyModifier modifiers = KeyModifier::None, uint32_t nativeCode = 0)
            -> MockExpectations &
        {
            addKey(key, modifiers, nativeCode, true);
            return *this;
        }

        auto click(Key key, KeyModifier modifiers = KeyModifier::None, uint32_t nativeCode = 0)
            -> MockExpectations &
        {
            return press(key, modifiers, nativeCode).release(key, modifiers, nativeCode);
        }

        auto chord(KeyModifier modifiers, Key key, uint32_t nativeCode = 0) -> MockExpectations &
        {
            return click(key, modifiers, nativeCode);
        }

        template <typename Rep, typename Period>
        auto repeat(Key key, std::chrono::duration<Rep, Period> duration, EventFrequency frequency,
                    KeyModifier modifiers = KeyModifier::None, uint32_t nativeCode = 0)
            -> MockExpectations &
        {
            auto total       = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
            auto repeatCount = detail::interpolationStepCount(total, frequency);
            press(key, modifiers, nativeCode);
            for (auto index = 0U; index < repeatCount; ++index) {
                addKey(key, modifiers, nativeCode, false, true);
            }
            release(key, modifiers, nativeCode);
            return *this;
        }

        auto events() const -> const std::vector<InputEvent> & { return mEvents; }

        auto clear() -> void
        {
            mEvents.clear();
            mPointer = {};
        }

    private:
        auto addMouseMove() -> void
        {
            mEvents.emplace_back(MouseMoveEvent{
                .x           = mPointer.x,
                .y           = mPointer.y,
                .screenIndex = mPointer.screenIndex,
            });
        }

        auto addMouseButton(MouseButton button, bool release) -> void
        {
            mEvents.emplace_back(MouseButtonEvent{
                .x           = mPointer.x,
                .y           = mPointer.y,
                .screenIndex = mPointer.screenIndex,
                .button      = button,
                .release     = release,
            });
        }

        auto addKey(Key key, KeyModifier modifiers, uint32_t nativeCode, bool release,
                    bool repeat = false) -> void
        {
            mEvents.emplace_back(KeyEvent{
                .key        = key,
                .modifiers  = modifiers,
                .nativeCode = nativeCode,
                .repeat     = repeat,
                .release    = release,
            });
        }

        detail::MockPointerState mPointer;
        std::vector<InputEvent>  mEvents;
    };

    struct MockVerification {
        bool        matched = false;
        std::string description;

        explicit operator bool() const { return matched; }
    };

    class MockInputCapture final : public InputCapture {
    public:
        auto initialize() -> IoTask<void> override
        {
            auto [sender, receiver] = ilias::mpsc::channel<InputEvent>(128);
            mSender                 = std::move(sender);
            mReceiver               = std::move(receiver);
            co_return {};
        }

        auto shutdown() -> Task<void> override
        {
            mSender   = {};
            mReceiver = {};
            co_return;
        }

        auto nextEvent() -> Task<InputEvent> override
        {
            if (auto event = co_await mReceiver.recv()) {
                co_return std::move(*event);
            }
            throw std::runtime_error("MockInputCapture::nextEvent called after shutdown");
        }

        auto setRemoteControlActive(bool active) -> IoResult<void> override
        {
            mRemoteControlActive = active;
            return {};
        }

        auto moveLocalCursor(uint32_t screenIndex, int32_t x, int32_t y) -> IoResult<void> override
        {
            mLastCursorMove = MockCursorMove{
                .screenIndex = screenIndex,
                .x           = x,
                .y           = y,
            };
            return {};
        }

        auto remoteControlActive() const -> bool { return mRemoteControlActive; }

        auto lastCursorMove() const -> std::optional<MockCursorMove> { return mLastCursorMove; }

        auto push(InputEvent event) -> bool
        {
            if (!mSender) {
                return false;
            }
            return static_cast<bool>(mSender.trySend(std::move(event)));
        }

    private:
        ilias::mpsc::Sender<InputEvent>   mSender;
        ilias::mpsc::Receiver<InputEvent> mReceiver;
        bool                              mRemoteControlActive = false;
        std::optional<MockCursorMove>     mLastCursorMove;
    };

    class MockInputInjector final : public InputInjector {
    public:
        auto initialize() -> IoTask<void> override
        {
            auto lock    = std::scoped_lock(mMutex);
            mInitialized = true;
            co_return {};
        }

        auto shutdown() -> Task<void> override
        {
            auto lock    = std::scoped_lock(mMutex);
            mInitialized = false;
            co_return;
        }

        auto inject(const InputEvent &event) -> IoTask<void> override
        {
            auto lock = std::scoped_lock(mMutex);
            if (!mInitialized) {
                throw std::runtime_error("MockInputInjector::inject called before initialize");
            }
            mEvents.push_back(event);
            co_return {};
        }

        auto events() const -> std::vector<InputEvent>
        {
            auto lock = std::scoped_lock(mMutex);
            return mEvents;
        }

        auto clear() -> void
        {
            auto lock = std::scoped_lock(mMutex);
            mEvents.clear();
        }

    private:
        mutable std::mutex      mMutex;
        bool                    mInitialized = false;
        std::vector<InputEvent> mEvents;
    };

    class MockPlatform final : public Platform {
    public:
        explicit MockPlatform(std::vector<ScreenInfo> screens)
            : mScreens(std::move(screens)), mCapture(std::make_shared<MockInputCapture>()),
              mInjector(std::make_shared<MockInputInjector>())
        {
        }

        auto screens() const -> std::vector<ScreenInfo> override
        {
            auto lock = std::scoped_lock(mMutex);
            return mScreens;
        }

        auto setScreens(std::vector<ScreenInfo> screens) -> void
        {
            auto lock = std::scoped_lock(mMutex);
            mScreens  = std::move(screens);
        }

        auto createCapture() -> InputCapture::Ptr override { return mCapture; }

        auto createInjector() -> InputInjector::Ptr override { return mInjector; }

        auto capture() const -> std::shared_ptr<MockInputCapture> { return mCapture; }

        auto injector() const -> std::shared_ptr<MockInputInjector> { return mInjector; }

        auto actions() -> MockActions & { return mActions; }

        auto expect() -> MockExpectations & { return mExpectations; }

        auto play() -> Task<bool>
        {
            auto steps    = mActions.steps();
            auto interval = detail::eventInterval(mActions.eventFrequency());
            for (const auto &step : steps) {
                if (const auto *delay = std::get_if<MockDelay>(&step)) {
                    interval = delay->duration;
                    continue;
                }
                co_await ilias::sleep(interval);
                const auto *event = std::get_if<InputEvent>(&step);
                if (!event || !mCapture->push(*event)) {
                    co_return false;
                }
                interval = detail::eventInterval(mActions.eventFrequency());
            }
            co_return true;
        }

        template <typename Rep, typename Period>
        auto waitForExpected(std::chrono::duration<Rep, Period> timeout) -> Task<bool>
        {
            using namespace std::chrono_literals;
            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(timeout);
            while (remaining > 0ms) {
                if (mInjector->events().size() >= mExpectations.events().size()) {
                    co_return true;
                }
                co_await ilias::sleep(1ms);
                remaining -= 1ms;
            }
            co_return mInjector->events().size() >= mExpectations.events().size();
        }

        auto verify() const -> MockVerification
        {
            auto        actual   = mInjector->events();
            const auto &expected = mExpectations.events();
            if (actual.size() != expected.size()) {
                return {
                    .matched     = false,
                    .description = fmtlib::format("expected {} injected event(s), received {}",
                                                  expected.size(), actual.size()),
                };
            }
            for (auto index = 0U; index < expected.size(); ++index) {
                if (!detail::sameEvent(expected[index], actual[index])) {
                    return {
                        .matched     = false,
                        .description = fmtlib::format("event {} mismatch: expected {}, received {}",
                                                      index, expected[index], actual[index]),
                    };
                }
            }
            return {.matched = true, .description = "all expected input events were received"};
        }

    private:
        mutable std::mutex                 mMutex;
        std::vector<ScreenInfo>            mScreens;
        std::shared_ptr<MockInputCapture>  mCapture;
        std::shared_ptr<MockInputInjector> mInjector;
        MockActions                        mActions;
        MockExpectations                   mExpectations;
    };

} // namespace mks::test
