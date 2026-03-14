#pragma once

#include <ilias/task.hpp>
#include <ilias/net.hpp>
#include <ilias/io.hpp>

#include "../platform/platform.hpp"
#include "../core/key.hpp"

namespace mksync::server {

// Import types
using ilias::IPEndpoint;
using ilias::TcpStream;
using ilias::IoTask;
using ilias::Task;
using ilias::Err;

using platform::InputCapture;
using platform::Platform;

/**
 * @brief The server class, collect the input and send it to the client
 * 
 */
class Server {
public:
    Server(const Server &) = delete;
    Server();
    ~Server();

    /**
     * @brief The main loop of the server, it will run infinitely until cancellation
     * 
     * @return IoTask<void> 
     */
    auto main() -> IoTask<void>;
private:
    // Collect events from the input capture
    auto collectEvents() -> IoTask<void>;

    // Accept the client connection
    auto networkAccept() -> IoTask<void>;

    // Handle the client connection
    auto networkHandle(TcpStream stream) -> Task<void>;

    Platform::Ptr     mPlatform;
    InputCapture::Ptr mInputCapture;

    // Configuration ...
    IPEndpoint        mBind;
};

} // namespace mksync::server