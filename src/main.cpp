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
        switch (event->type) {
            case InputEvent::MouseMove:
                spdlog::info("Mouse move: at {}: {}, {}", event->mouse.screenIndex, event->mouse.x, event->mouse.y);
                break;
            case InputEvent::MousePress:
                spdlog::info("Mouse press: at {}: {}", event->mouse.screenIndex, event->mouse.button);
                break;
            case InputEvent::MouseRelease:
                spdlog::info("Mouse release: at {}: {}", event->mouse.screenIndex, event->mouse.button);
                break;
        }
    }
}

void ilias_main() {
    auto capture = mksync::platform::createInputCapture();
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