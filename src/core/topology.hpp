#pragma once

#include "preinclude.hpp"
#include "events.hpp"
#include "refl/this_error.hpp"
#include <compare>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

MKS_BEGIN

enum class TopologyError {
    Ok = 0,
    InvalidScreenRect,
    DuplicateScreen,
    CellOccupied,
    UnknownScreen,
    MissingNeighbor,
};
THIS_ERROR(TopologyError);

enum class Edge {
    Left,
    Right,
    Top,
    Bottom,
};
FORMATTER(Edge);

struct GridPosition {
    // Integer topology cell. This is not a pixel coordinate and does not
    // encode the real size of the monitor.
    int32_t x = 0;
    int32_t y = 0;

    auto operator<=>(const GridPosition &) const = default;
};
FORMATTER(GridPosition);

struct ScreenKey {
    // Stable owner id. Prefer the peer machineId; endpoint strings are only a
    // fallback for clients that do not send one yet.
    std::string ownerId;
    uint32_t screenIndex = 0;

    auto operator<=>(const ScreenKey &) const = default;
};
FORMATTER(ScreenKey);

struct TopologyScreen {
    ScreenKey key;
    GridPosition cell;
    ScreenInfo info;
    bool local = false;
};
FORMATTER(TopologyScreen);

struct ScreenPoint {
    ScreenKey key;
    // Real pixel coordinate inside the screen identified by key.
    // We intentionally do not keep a long-lived normalized [0, 1] cursor.
    int32_t x = 0;
    int32_t y = 0;

    auto operator==(const ScreenPoint &) const -> bool = default;
};
FORMATTER(ScreenPoint);

class ScreenTopology {
public:
    auto addScreen(TopologyScreen screen) -> IoResult<void>;
    auto removeOwner(std::string_view ownerId) -> void;

    auto findScreen(const ScreenKey &key) const -> const TopologyScreen *;
    auto screens() const -> std::vector<TopologyScreen>;

    auto findNeighbor(const ScreenKey &key, Edge edge) const -> std::optional<ScreenKey>;
    auto hitEdge(const ScreenPoint &point) const -> std::optional<Edge>;
    auto mapEntryPoint(const ScreenPoint &from, Edge edge) const -> IoResult<ScreenPoint>;

private:
    std::map<ScreenKey, TopologyScreen> mScreens;
    std::map<GridPosition, ScreenKey> mCells;
};

MKS_END

REFL_REGISTER_FMT_FORMATTER(mks::TopologyError);
REFL_REGISTER_FMT_FORMATTER(mks::Edge);
REFL_REGISTER_FMT_FORMATTER(mks::GridPosition);
REFL_REGISTER_FMT_FORMATTER(mks::ScreenKey);
REFL_REGISTER_FMT_FORMATTER(mks::TopologyScreen);
REFL_REGISTER_FMT_FORMATTER(mks::ScreenPoint);
