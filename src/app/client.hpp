#pragma once

#include <ilias/net.hpp>
#include <ilias/task.hpp>

#include <optional>

#include "../platform/platform.hpp"
#include "../transport/session.hpp"

namespace mksync::app {

class ClientApp {
public:
    auto run(const ilias::IPEndpoint &endpoint) -> ilias::IoTask<void>;

private:
    auto initialize() -> ilias::IoTask<void>;
    auto shutdown() -> ilias::Task<void>;
    auto sessionLoop(transport::BufferedTcpStream &stream) -> ilias::IoTask<void>;

    platform::Platform::Ptr mPlatform;
    platform::InputInjector::Ptr mInjector;
    bool mRemoteFocusActive = false;
    std::optional<uint32_t> mRemoteFocusScreen;
};

} // namespace mksync::app
