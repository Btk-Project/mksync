#include <ilias/platform.hpp>
#include <ilias/signal.hpp>
#include <ilias/task.hpp>

#include <span>
#include <string_view>
#include <vector>

#include "app/client.hpp"
#include "app/host.hpp"

void ilias_main(int argc, char **argv) {
    std::vector<std::string_view> args;
    args.reserve(argc > 1 ? static_cast<size_t>(argc - 1) : 0);
    for (int i = 1; i < argc; ++i) {
        auto value = std::string_view(argv[i]);
        if (value == "--") {
            continue;
        }
        args.push_back(value);
    }

    if (args.size() >= 2 && args[0] == "client") {
        auto endpoint = ilias::IPEndpoint(args[1].data());
        auto app = mksync::app::ClientApp {};
        auto _ = co_await ilias::whenAny(
            app.run(endpoint),
            ilias::signal::ctrlC()
        );
        co_return;
    }

    auto bind = !args.empty() ? ilias::IPEndpoint(args[0].data()) : ilias::IPEndpoint("0.0.0.0:24800");
    auto app = mksync::app::HostApp {};
    auto _ = co_await ilias::whenAny(
        app.run(bind),
        ilias::signal::ctrlC()
    );
    co_return;
}
