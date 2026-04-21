#pragma once
#include <mksync/_proto/_config.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "../platform/platform.hpp"

namespace mksync::domain {

enum class Edge {
    Left,
    Right,
    Top,
    Bottom,
};

struct ScreenId {
    std::string node;
    uint32_t index = 0;

    auto operator ==(const ScreenId &) const -> bool = default;
};

struct ScreenIdHash {
    auto operator ()(const ScreenId &id) const noexcept -> size_t {
        return std::hash<std::string> {}(id.node) ^ (static_cast<size_t>(id.index) << 1);
    }
};

struct ScreenBounds {
    int32_t x = 0;
    int32_t y = 0;
    int32_t width = 0;
    int32_t height = 0;
};

class ScreenTopology {
public:
    auto clear() -> void;
    auto setLocalNode(std::string nodeName) -> void;
    auto localNode() const -> std::string_view;

    auto upsertScreen(ScreenId id, ScreenBounds bounds) -> void;
    auto removeScreen(const ScreenId &id) -> bool;
    auto screen(const ScreenId &id) const -> const ScreenBounds *;
    auto allScreens() const -> std::vector<ScreenId>;

    auto setLink(ScreenId from, Edge edge, ScreenId to, bool bidirectional = false) -> void;
    auto neighbor(const ScreenId &from, Edge edge) const -> std::optional<ScreenId>;

    auto isLocal(const ScreenId &id) const -> bool;
    auto localScreen(uint32_t index) const -> std::optional<ScreenId>;

    auto loadLocalScreens(std::span<const platform::ScreenInfo> screens) -> void;

    static auto edgeAt(const ScreenBounds &bounds, int32_t x, int32_t y, int32_t threshold = 0) -> std::optional<Edge>;
    static auto opposite(Edge edge) -> Edge;

private:
    struct ScreenEntry {
        ScreenBounds bounds;
        std::unordered_map<Edge, ScreenId> neighbors;
    };

    std::string mLocalNode = "local";
    std::unordered_map<ScreenId, ScreenEntry, ScreenIdHash> mScreens;
};

} // namespace mksync::domain
