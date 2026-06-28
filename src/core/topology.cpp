#include "topology.hpp"
#include <algorithm>
#include <cmath>

MKS_BEGIN

THIS_ERROR_IMPL(TopologyError);

namespace {

auto isValidRect(const ScreenInfo &info) -> bool {
    return info.width > 0 && info.height > 0;
}

auto neighborCell(GridPosition cell, Edge edge) -> GridPosition {
    switch (edge) {
        case Edge::Left:
            --cell.x;
            break;
        case Edge::Right:
            ++cell.x;
            break;
        case Edge::Top:
            --cell.y;
            break;
        case Edge::Bottom:
            ++cell.y;
            break;
    }
    return cell;
}

auto entryCoordinate(int32_t source, int32_t sourceExtent, int32_t targetExtent) -> int32_t {
    if (sourceExtent <= 1 || targetExtent <= 1) {
        return 0;
    }

    const auto sourceSpan = sourceExtent - 1;
    const auto targetSpan = targetExtent - 1;
    const auto clamped = std::clamp(source, 0, sourceSpan);
    const auto mapped = std::lround(
        static_cast<double>(clamped) * static_cast<double>(targetSpan)
        / static_cast<double>(sourceSpan)
    );
    return std::clamp(static_cast<int32_t>(mapped), 0, targetSpan);
}

} // namespace

auto ScreenTopology::addScreen(TopologyScreen screen) -> IoResult<void> {
    if (!isValidRect(screen.info)) {
        return Err(TopologyError::InvalidScreenRect);
    }
    if (mScreens.contains(screen.key)) {
        return Err(TopologyError::DuplicateScreen);
    }
    if (mCells.contains(screen.cell)) {
        return Err(TopologyError::CellOccupied);
    }

    const auto key = screen.key;
    const auto cell = screen.cell;
    mScreens.emplace(key, std::move(screen));
    mCells.emplace(cell, key);
    return {};
}

auto ScreenTopology::removeOwner(std::string_view ownerId) -> void {
    for (auto it = mScreens.begin(); it != mScreens.end();) {
        if (it->first.ownerId == ownerId) {
            mCells.erase(it->second.cell);
            it = mScreens.erase(it);
        }
        else {
            ++it;
        }
    }
}

auto ScreenTopology::findScreen(const ScreenKey &key) const -> const TopologyScreen * {
    auto it = mScreens.find(key);
    if (it == mScreens.end()) {
        return nullptr;
    }
    return &it->second;
}

auto ScreenTopology::screens() const -> std::vector<TopologyScreen> {
    std::vector<TopologyScreen> result;
    result.reserve(mScreens.size());
    for (const auto &[_, screen] : mScreens) {
        result.push_back(screen);
    }
    return result;
}

auto ScreenTopology::findNeighbor(const ScreenKey &key, Edge edge) const -> std::optional<ScreenKey> {
    const auto *screen = findScreen(key);
    if (screen == nullptr) {
        return std::nullopt;
    }

    auto it = mCells.find(neighborCell(screen->cell, edge));
    if (it == mCells.end()) {
        return std::nullopt;
    }
    return it->second;
}

auto ScreenTopology::hitEdge(const ScreenPoint &point) const -> std::optional<Edge> {
    const auto *screen = findScreen(point.key);
    if (screen == nullptr) {
        return std::nullopt;
    }

    const auto &info = screen->info;
    if (point.x <= 0) {
        return Edge::Left;
    }
    if (point.x >= info.width - 1) {
        return Edge::Right;
    }
    if (point.y <= 0) {
        return Edge::Top;
    }
    if (point.y >= info.height - 1) {
        return Edge::Bottom;
    }
    return std::nullopt;
}

auto ScreenTopology::mapEntryPoint(const ScreenPoint &from, Edge edge) const -> IoResult<ScreenPoint> {
    const auto *source = findScreen(from.key);
    if (source == nullptr) {
        return Err(TopologyError::UnknownScreen);
    }

    auto targetKey = findNeighbor(from.key, edge);
    if (!targetKey) {
        return Err(TopologyError::MissingNeighbor);
    }

    const auto *target = findScreen(*targetKey);
    if (target == nullptr) {
        return Err(TopologyError::UnknownScreen);
    }

    const auto &sourceInfo = source->info;
    const auto &targetInfo = target->info;
    ScreenPoint result {
        .key = *targetKey,
        .x = 0,
        .y = 0,
    };

    switch (edge) {
        case Edge::Left:
            result.x = targetInfo.width - 1;
            result.y = entryCoordinate(from.y, sourceInfo.height, targetInfo.height);
            break;
        case Edge::Right:
            result.x = 0;
            result.y = entryCoordinate(from.y, sourceInfo.height, targetInfo.height);
            break;
        case Edge::Top:
            result.x = entryCoordinate(from.x, sourceInfo.width, targetInfo.width);
            result.y = targetInfo.height - 1;
            break;
        case Edge::Bottom:
            result.x = entryCoordinate(from.x, sourceInfo.width, targetInfo.width);
            result.y = 0;
            break;
    }

    return result;
}

MKS_END
