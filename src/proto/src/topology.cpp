#include "pch.hpp"
#include "topology.hpp"

#include <algorithm>

namespace mksync::domain {

auto ScreenTopology::clear() -> void {
    mScreens.clear();
}

auto ScreenTopology::setLocalNode(std::string nodeName) -> void {
    mLocalNode = std::move(nodeName);
}

auto ScreenTopology::localNode() const -> std::string_view {
    return mLocalNode;
}

auto ScreenTopology::upsertScreen(ScreenId id, ScreenBounds bounds) -> void {
    mScreens[std::move(id)].bounds = bounds;
}

auto ScreenTopology::removeScreen(const ScreenId &id) -> bool {
    auto erased = mScreens.erase(id) != 0;
    if (!erased) {
        return false;
    }

    for (auto &[_, entry] : mScreens) {
        for (auto it = entry.neighbors.begin(); it != entry.neighbors.end();) {
            if (it->second == id) {
                it = entry.neighbors.erase(it);
            }
            else {
                ++it;
            }
        }
    }
    return true;
}

auto ScreenTopology::screen(const ScreenId &id) const -> const ScreenBounds * {
    if (auto it = mScreens.find(id); it != mScreens.end()) {
        return &it->second.bounds;
    }
    return nullptr;
}

auto ScreenTopology::allScreens() const -> std::vector<ScreenId> {
    std::vector<ScreenId> result;
    result.reserve(mScreens.size());
    for (const auto &[id, _] : mScreens) {
        result.push_back(id);
    }
    std::sort(result.begin(), result.end(), [](const auto &lhs, const auto &rhs) {
        return lhs.node == rhs.node ? lhs.index < rhs.index : lhs.node < rhs.node;
    });
    return result;
}

auto ScreenTopology::setLink(ScreenId from, Edge edge, ScreenId to, bool bidirectional) -> void {
    mScreens[from].neighbors[edge] = to;
    if (bidirectional) {
        mScreens[to].neighbors[opposite(edge)] = std::move(from);
    }
}

auto ScreenTopology::neighbor(const ScreenId &from, Edge edge) const -> std::optional<ScreenId> {
    auto it = mScreens.find(from);
    if (it == mScreens.end()) {
        return std::nullopt;
    }
    if (auto neighborIt = it->second.neighbors.find(edge); neighborIt != it->second.neighbors.end()) {
        return neighborIt->second;
    }
    return std::nullopt;
}

auto ScreenTopology::isLocal(const ScreenId &id) const -> bool {
    return id.node == mLocalNode;
}

auto ScreenTopology::localScreen(uint32_t index) const -> std::optional<ScreenId> {
    auto id = ScreenId {std::string(mLocalNode), index};
    return mScreens.contains(id) ? std::optional<ScreenId>(std::move(id)) : std::nullopt;
}

auto ScreenTopology::loadLocalScreens(std::span<const platform::ScreenInfo> screens) -> void {
    std::vector<ScreenId> toRemove;
    for (const auto &[id, _] : mScreens) {
        if (isLocal(id)) {
            toRemove.push_back(id);
        }
    }
    for (const auto &id : toRemove) {
        removeScreen(id);
    }

    for (size_t i = 0; i < screens.size(); ++i) {
        const auto &screen = screens[i];
        upsertScreen(
            ScreenId {std::string(mLocalNode), static_cast<uint32_t>(i)},
            ScreenBounds {
                .x = screen.x,
                .y = screen.y,
                .width = screen.width,
                .height = screen.height,
            }
        );
    }
}

auto ScreenTopology::edgeAt(const ScreenBounds &bounds, int32_t x, int32_t y, int32_t threshold) -> std::optional<Edge> {
    if (bounds.width <= 0 || bounds.height <= 0) {
        return std::nullopt;
    }

    if (x <= threshold) {
        return Edge::Left;
    }
    if (x >= bounds.width - 1 - threshold) {
        return Edge::Right;
    }
    if (y <= threshold) {
        return Edge::Top;
    }
    if (y >= bounds.height - 1 - threshold) {
        return Edge::Bottom;
    }
    return std::nullopt;
}

auto ScreenTopology::opposite(Edge edge) -> Edge {
    switch (edge) {
        case Edge::Left: return Edge::Right;
        case Edge::Right: return Edge::Left;
        case Edge::Top: return Edge::Bottom;
        case Edge::Bottom: return Edge::Top;
    }
    return Edge::Left;
}

} // namespace mksync::domain
