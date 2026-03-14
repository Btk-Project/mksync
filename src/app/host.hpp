#pragma once

#include <ilias/net.hpp>
#include <ilias/sync.hpp>
#include <ilias/task.hpp>

#include <optional>
#include <string>
#include <vector>

#include "../domain/focus.hpp"
#include "../domain/topology.hpp"
#include "../platform/platform.hpp"
#include "../proto/message.hpp"
#include "../transport/session.hpp"

namespace mksync::app {

class HostApp {
public:
    auto run(const ilias::IPEndpoint &bind = ilias::IPEndpoint("0.0.0.0:24800")) -> ilias::IoTask<void>;

private:
    auto initialize() -> ilias::IoTask<void>;
    auto shutdown() -> ilias::Task<void>;
    auto eventLoop() -> ilias::IoTask<void>;
    auto listenerLoop() -> ilias::IoTask<void>;
    auto handleSession(ilias::TcpStream tcp) -> ilias::IoTask<void>;
    auto sessionReadLoop(transport::BufferedTcpStream &stream, ilias::mpsc::Sender<proto::Frame> sender) -> ilias::IoTask<void>;
    auto sessionWriteLoop(transport::BufferedTcpStream &stream, ilias::mpsc::Receiver<proto::Frame> receiver) -> ilias::IoTask<void>;

    auto handleEvent(const mksync::platform::InputEvent &event) -> void;
    auto publishFrame(proto::Frame frame) -> void;
    auto interceptLocalHotkey(const mksync::platform::InputEvent &event) -> bool;
    auto makeRemoteEvent(const mksync::platform::InputEvent &event, std::optional<domain::Edge> edge = std::nullopt) const -> std::optional<mksync::platform::InputEvent>;
    auto currentRemoteTarget() const -> std::optional<mksync::domain::FocusTarget>;
    auto reloadLocalTopology() -> void;
    auto updateRemoteTopology(std::string nodeName, std::span<const platform::ScreenInfo> screens) -> void;
    auto clearRemoteTopology() -> void;
    auto rebuildTopology() -> void;
    auto linkOrderedScreens(std::span<const domain::ScreenId> ordered, domain::Edge edge) -> void;

    mksync::platform::Platform::Ptr mPlatform;
    mksync::platform::InputCapture::Ptr mCapture;
    mksync::platform::InputInjector::Ptr mInjector;

    mksync::domain::ScreenTopology mTopology;
    mksync::domain::FocusController mFocus;

    ilias::IPEndpoint mBind {"0.0.0.0:24800"};
    ilias::mpsc::Sender<proto::Frame> mActiveSender;

    std::string mRemoteNode;
    std::vector<platform::ScreenInfo> mRemoteScreens;
};

} // namespace mksync::app
