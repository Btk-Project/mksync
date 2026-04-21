#pragma once
#include <mksync/_proto/_config.hpp>

#include <optional>

#include "topology.hpp"

namespace mksync::domain {

enum class FocusState {
    LocalActive,
    RemoteActive,
    Locked,
};

struct FocusTarget {
    ScreenId screen;
    bool local = false;
};

struct RouteDecision {
    enum class Action {
        Noop,
        KeepLocal,
        ForwardToRemote,
        SwitchToRemote,
        ReturnLocal,
    } action = Action::Noop;

    FocusState nextState = FocusState::LocalActive;
    std::optional<FocusTarget> target;
    std::optional<Edge> edge;
};

class FocusController {
public:
    auto setTopology(const ScreenTopology *topology) -> void;
    auto setEdgeThreshold(int32_t threshold) -> void;
    auto state() const -> FocusState;

    auto lock() -> void;
    auto unlock() -> void;

    auto activateLocal(ScreenId screen) -> void;
    auto activateRemote(ScreenId screen) -> void;

    auto activeTarget() const -> std::optional<FocusTarget>;
    auto handlePointer(const ScreenId &screen, int32_t x, int32_t y) -> RouteDecision;
    auto onRemoteDisconnected() -> RouteDecision;

private:
    const ScreenTopology *mTopology = nullptr;
    FocusState mState = FocusState::LocalActive;
    std::optional<FocusTarget> mTarget;
    int32_t mEdgeThreshold = 0;
};

} // namespace mksync::domain
