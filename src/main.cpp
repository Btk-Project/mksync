#include <ilias/platform.hpp>
#include <ilias/signal.hpp>
#include <ilias/task.hpp>
#include <spdlog/spdlog.h>

#include "platform/platform.hpp"

using mksync::platform::InputCapture;

auto loop(InputCapture *capture) -> ilias::Task<void> {
    while (true) {
        auto event = co_await capture->nextEvent();
        if (!event) {
            SPDLOG_WARN("Input capture stopped: {}", event.error().message());
            break;
        }
        SPDLOG_INFO("Event: {}", *event);
    }
    co_return;
}

void ilias_main() {
    auto platform = mksync::platform::createPlatform();
    if (!platform) {
        spdlog::error("Failed to create platform backend");
        co_return;
    }

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
