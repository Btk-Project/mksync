#pragma once

#include <ilias/net.hpp>
#include <ilias/sync.hpp>
#include <ilias/task.hpp>

#include <optional>

#include "../domain/topology.hpp"
#include "../platform/platform.hpp"
#include "../transport/session.hpp"

namespace mksync::app {

class ClientApp {
public:
    auto run(const ilias::IPEndpoint &endpoint) -> ilias::IoTask<void>;

private:
    auto initialize() -> ilias::IoTask<void>;
    auto shutdown() -> ilias::Task<void>;
    auto sessionReadLoop(transport::BufferedTcpStream &stream, ilias::mpsc::Sender<proto::Frame> sender) -> ilias::IoTask<void>;
    auto sessionWriteLoop(transport::BufferedTcpStream &stream, ilias::mpsc::Receiver<proto::Frame> receiver) -> ilias::IoTask<void>;
    auto captureLoop(ilias::mpsc::Sender<proto::Frame> sender) -> ilias::IoTask<void>;

    auto handleCapturedEvent(const platform::InputEvent &event, ilias::mpsc::Sender<proto::Frame> sender) -> void;
    auto clearRemoteFocus() -> void;
    auto setRemoteFocus(uint32_t screenIndex, domain::Edge entryEdge) -> void;
    auto shouldReturnToHost(const platform::InputEvent::MouseMoveData &data, const platform::ScreenInfo &screen, domain::Edge edge) const -> bool;
    auto movedAwayFromReturnEdge(const platform::InputEvent::MouseMoveData &data, const platform::ScreenInfo &screen, domain::Edge edge) const -> bool;

    platform::Platform::Ptr mPlatform;
    platform::InputCapture::Ptr mCapture;
    platform::InputInjector::Ptr mInjector;

    bool mRemoteFocusActive = false;
    std::optional<uint32_t> mRemoteFocusScreen;
    std::optional<domain::Edge> mRemoteEntryEdge;
    bool mReturnEdgeArmed = false;
    int32_t mReturnEdgeThreshold = 8;
};

} // namespace mksync::app
