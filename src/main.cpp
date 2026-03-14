#include <ilias/platform.hpp>
#include <ilias/signal.hpp>
#include <ilias/task.hpp>
#include <spdlog/spdlog.h>
#include "platform/platform.hpp"

using namespace std::literals;
using mksync::platform::InputCapture;
using mksync::platform::InputEvent;

auto loop(InputCapture *capture) -> ilias::Task<void> {
    while (true) {
        auto event = co_await capture->nextEvent();
        if (!event) {
            break;
        }
        SPDLOG_INFO("Event: {}", *event);
    }
}

void ilias_main() {
    auto platform = mksync::platform::createPlatform();
    auto capture = platform->createInputCapture();
    if (!capture || !co_await capture->initialize()) {
        spdlog::error("Failed to create input capture");
        co_return;
    }

    auto _ = co_await ilias::whenAny(
        loop(capture.get()),
        ilias::signal::ctrlC()
    );

    co_await capture->shutdown();
}