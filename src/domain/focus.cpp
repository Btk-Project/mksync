#include "pch.hpp"
#include "focus.hpp"

namespace mksync::domain {

auto FocusController::setTopology(const ScreenTopology *topology) -> void {
    mTopology = topology;
}

auto FocusController::setEdgeThreshold(int32_t threshold) -> void {
    mEdgeThreshold = threshold;
}

auto FocusController::state() const -> FocusState {
    return mState;
}

auto FocusController::lock() -> void {
    mState = FocusState::Locked;
}

auto FocusController::unlock() -> void {
    if (mTarget && !mTarget->local) {
        mState = FocusState::RemoteActive;
    }
    else {
        mState = FocusState::LocalActive;
    }
}

auto FocusController::activateLocal(ScreenId screen) -> void {
    mState = FocusState::LocalActive;
    mTarget = FocusTarget {.screen = std::move(screen), .local = true};
}

auto FocusController::activateRemote(ScreenId screen) -> void {
    mState = FocusState::RemoteActive;
    mTarget = FocusTarget {.screen = std::move(screen), .local = false};
}

auto FocusController::activeTarget() const -> std::optional<FocusTarget> {
    return mTarget;
}

auto FocusController::handlePointer(const ScreenId &screen, int32_t x, int32_t y) -> RouteDecision {
    if (mState == FocusState::Locked || mTopology == nullptr) {
        return {.action = RouteDecision::Action::Noop, .nextState = mState, .target = mTarget};
    }

    const auto *bounds = mTopology->screen(screen);
    if (bounds == nullptr) {
        return {.action = RouteDecision::Action::Noop, .nextState = mState, .target = mTarget};
    }

    auto edge = ScreenTopology::edgeAt(*bounds, x, y, mEdgeThreshold);
    if (!edge) {
        return {
            .action = mState == FocusState::RemoteActive ? RouteDecision::Action::ForwardToRemote : RouteDecision::Action::KeepLocal,
            .nextState = mState,
            .target = mTarget,
        };
    }

    auto next = mTopology->neighbor(screen, *edge);
    if (!next) {
        return {
            .action = mState == FocusState::RemoteActive ? RouteDecision::Action::ForwardToRemote : RouteDecision::Action::KeepLocal,
            .nextState = mState,
            .target = mTarget,
            .edge = edge,
        };
    }

    auto nextTarget = FocusTarget {.screen = *next, .local = mTopology->isLocal(*next)};
    if (mTarget && mTarget->screen == nextTarget.screen && mTarget->local == nextTarget.local) {
        return {
            .action = nextTarget.local ? RouteDecision::Action::KeepLocal : RouteDecision::Action::ForwardToRemote,
            .nextState = mState,
            .target = nextTarget,
            .edge = edge,
        };
    }

    mTarget = nextTarget;
    mState = nextTarget.local ? FocusState::LocalActive : FocusState::RemoteActive;

    return {
        .action = nextTarget.local ? RouteDecision::Action::ReturnLocal : RouteDecision::Action::SwitchToRemote,
        .nextState = mState,
        .target = nextTarget,
        .edge = edge,
    };
}

auto FocusController::onRemoteDisconnected() -> RouteDecision {
    if (mState != FocusState::RemoteActive) {
        return {.action = RouteDecision::Action::Noop, .nextState = mState, .target = mTarget};
    }

    if (mTarget && mTopology && mTopology->isLocal(mTarget->screen)) {
        mState = FocusState::LocalActive;
        return {
            .action = RouteDecision::Action::ReturnLocal,
            .nextState = mState,
            .target = mTarget,
        };
    }

    mState = FocusState::LocalActive;
    return {
        .action = RouteDecision::Action::ReturnLocal,
        .nextState = mState,
        .target = mTarget,
    };
}

} // namespace mksync::domain

