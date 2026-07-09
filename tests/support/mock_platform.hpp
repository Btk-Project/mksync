#pragma once

#include "platform/platform.hpp"
#include <ilias/sync.hpp>
#include <mutex>
#include <vector>

namespace mks::test {

class MockInputCapture final : public InputCapture {
public:
    auto initialize() -> IoTask<void> override {
        auto [sender, receiver] = ilias::mpsc::channel<InputEvent>(128);
        mSender = std::move(sender);
        mReceiver = std::move(receiver);
        co_return {};
    }

    auto shutdown() -> Task<void> override {
        mSender = {};
        mReceiver = {};
        co_return;
    }

    auto nextEvent() -> Task<InputEvent> override {
        if (auto event = co_await mReceiver.recv()) {
            co_return std::move(*event);
        }
        throw std::runtime_error("MockInputCapture::nextEvent called after shutdown");
    }

    auto setRemoteControlActive(bool active) -> IoResult<void> override {
        mRemoteControlActive = active;
        return {};
    }

    auto remoteControlActive() const -> bool {
        return mRemoteControlActive;
    }

    auto push(InputEvent event) -> bool {
        if (!mSender) {
            return false;
        }
        return static_cast<bool>(mSender.trySend(std::move(event)));
    }

private:
    ilias::mpsc::Sender<InputEvent> mSender;
    ilias::mpsc::Receiver<InputEvent> mReceiver;
    bool mRemoteControlActive = false;
};

class MockInputInjector final : public InputInjector {
public:
    auto initialize() -> IoTask<void> override {
        auto lock = std::scoped_lock(mMutex);
        mInitialized = true;
        co_return {};
    }

    auto shutdown() -> Task<void> override {
        auto lock = std::scoped_lock(mMutex);
        mInitialized = false;
        co_return;
    }

    auto inject(const InputEvent &event) -> IoTask<void> override {
        auto lock = std::scoped_lock(mMutex);
        if (!mInitialized) {
            throw std::runtime_error("MockInputInjector::inject called before initialize");
        }
        mEvents.push_back(event);
        co_return {};
    }

    auto events() const -> std::vector<InputEvent> {
        auto lock = std::scoped_lock(mMutex);
        return mEvents;
    }

    auto clear() -> void {
        auto lock = std::scoped_lock(mMutex);
        mEvents.clear();
    }

private:
    mutable std::mutex mMutex;
    bool mInitialized = false;
    std::vector<InputEvent> mEvents;
};

class MockPlatform final : public Platform {
public:
    explicit MockPlatform(std::vector<ScreenInfo> screens)
        : mScreens(std::move(screens)),
          mCapture(std::make_shared<MockInputCapture>()),
          mInjector(std::make_shared<MockInputInjector>()) {}

    auto screens() const -> std::vector<ScreenInfo> override {
        auto lock = std::scoped_lock(mMutex);
        return mScreens;
    }

    auto setScreens(std::vector<ScreenInfo> screens) -> void {
        auto lock = std::scoped_lock(mMutex);
        mScreens = std::move(screens);
    }

    auto createCapture() -> InputCapture::Ptr override {
        return mCapture;
    }

    auto createInjector() -> InputInjector::Ptr override {
        return mInjector;
    }

    auto capture() const -> std::shared_ptr<MockInputCapture> {
        return mCapture;
    }

    auto injector() const -> std::shared_ptr<MockInputInjector> {
        return mInjector;
    }

private:
    mutable std::mutex mMutex;
    std::vector<ScreenInfo> mScreens;
    std::shared_ptr<MockInputCapture> mCapture;
    std::shared_ptr<MockInputInjector> mInjector;
};

} // namespace mks::test
