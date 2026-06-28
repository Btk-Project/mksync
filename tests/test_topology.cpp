#include "core/topology.hpp"
#include <gtest/gtest.h>

namespace {

auto makeKey(std::string ownerId, uint32_t screenIndex = 0) -> mks::ScreenKey {
    return mks::ScreenKey {
        .ownerId = std::move(ownerId),
        .screenIndex = screenIndex,
    };
}

auto makeScreen(
    mks::ScreenKey key,
    mks::GridPosition cell,
    int32_t width,
    int32_t height
) -> mks::TopologyScreen {
    auto name = key.ownerId;
    return mks::TopologyScreen {
        .key = std::move(key),
        .cell = cell,
        .info = mks::ScreenInfo {
            .x = 0,
            .y = 0,
            .width = width,
            .height = height,
            .dpi = 72,
            .name = std::move(name),
            .primary = false,
        },
        .local = false,
    };
}

} // namespace

TEST(ScreenTopology, FindsNeighborsByGridCell) {
    auto topology = mks::ScreenTopology {};
    const auto center = makeKey("center");
    const auto right = makeKey("right");
    const auto bottom = makeKey("bottom");

    ASSERT_TRUE(topology.addScreen(makeScreen(center, {0, 0}, 1920, 1080)).has_value());
    ASSERT_TRUE(topology.addScreen(makeScreen(right, {1, 0}, 2560, 1440)).has_value());
    ASSERT_TRUE(topology.addScreen(makeScreen(bottom, {0, 1}, 1280, 720)).has_value());

    EXPECT_EQ(topology.findNeighbor(center, mks::Edge::Right), right);
    EXPECT_EQ(topology.findNeighbor(right, mks::Edge::Left), center);
    EXPECT_EQ(topology.findNeighbor(center, mks::Edge::Bottom), bottom);
    EXPECT_EQ(topology.findNeighbor(bottom, mks::Edge::Top), center);
    EXPECT_EQ(topology.findNeighbor(center, mks::Edge::Left), std::nullopt);
}

TEST(ScreenTopology, RejectsDuplicateKeysAndCells) {
    auto topology = mks::ScreenTopology {};
    const auto first = makeKey("first");
    const auto second = makeKey("second");

    ASSERT_TRUE(topology.addScreen(makeScreen(first, {0, 0}, 1920, 1080)).has_value());

    auto duplicateKey = topology.addScreen(makeScreen(first, {1, 0}, 1920, 1080));
    ASSERT_FALSE(duplicateKey.has_value());
    EXPECT_EQ(
        duplicateKey.error(),
        mks::make_error_code(mks::TopologyError::DuplicateScreen)
    );

    auto duplicateCell = topology.addScreen(makeScreen(second, {0, 0}, 1920, 1080));
    ASSERT_FALSE(duplicateCell.has_value());
    EXPECT_EQ(
        duplicateCell.error(),
        mks::make_error_code(mks::TopologyError::CellOccupied)
    );
}

TEST(ScreenTopology, RejectsInvalidRects) {
    auto topology = mks::ScreenTopology {};

    auto invalid = topology.addScreen(makeScreen(makeKey("invalid"), {0, 0}, 0, 1080));
    ASSERT_FALSE(invalid.has_value());
    EXPECT_EQ(
        invalid.error(),
        mks::make_error_code(mks::TopologyError::InvalidScreenRect)
    );
}

TEST(ScreenTopology, DetectsEdgesFromRealScreenRect) {
    auto topology = mks::ScreenTopology {};
    const auto key = makeKey("screen");
    ASSERT_TRUE(topology.addScreen(makeScreen(key, {0, 0}, 1920, 1080)).has_value());

    EXPECT_EQ(topology.hitEdge({.key = key, .x = 0, .y = 500}), mks::Edge::Left);
    EXPECT_EQ(topology.hitEdge({.key = key, .x = 1919, .y = 500}), mks::Edge::Right);
    EXPECT_EQ(topology.hitEdge({.key = key, .x = 500, .y = 0}), mks::Edge::Top);
    EXPECT_EQ(topology.hitEdge({.key = key, .x = 500, .y = 1079}), mks::Edge::Bottom);
    EXPECT_EQ(topology.hitEdge({.key = key, .x = 500, .y = 500}), std::nullopt);
}

TEST(ScreenTopology, MapsEntryPointAcrossDifferentResolutions) {
    auto topology = mks::ScreenTopology {};
    const auto local = makeKey("local");
    const auto remote = makeKey("remote");

    ASSERT_TRUE(topology.addScreen(makeScreen(local, {0, 0}, 1920, 1080)).has_value());
    ASSERT_TRUE(topology.addScreen(makeScreen(remote, {1, 0}, 2560, 1440)).has_value());

    auto mapped = topology.mapEntryPoint(
        {.key = local, .x = 1919, .y = 810},
        mks::Edge::Right
    );
    ASSERT_TRUE(mapped.has_value()) << mapped.error().message();
    EXPECT_EQ(mapped->key, remote);
    EXPECT_EQ(mapped->x, 0);
    EXPECT_EQ(mapped->y, 1080);

    auto mappedBack = topology.mapEntryPoint(
        {.key = remote, .x = 0, .y = 1080},
        mks::Edge::Left
    );
    ASSERT_TRUE(mappedBack.has_value()) << mappedBack.error().message();
    EXPECT_EQ(mappedBack->key, local);
    EXPECT_EQ(mappedBack->x, 1919);
    EXPECT_EQ(mappedBack->y, 810);
}

TEST(ScreenTopology, MapsVerticalEntryPointAcrossDifferentResolutions) {
    auto topology = mks::ScreenTopology {};
    const auto top = makeKey("top");
    const auto bottom = makeKey("bottom");

    ASSERT_TRUE(topology.addScreen(makeScreen(top, {0, 0}, 1920, 1080)).has_value());
    ASSERT_TRUE(topology.addScreen(makeScreen(bottom, {0, 1}, 1280, 720)).has_value());

    auto mapped = topology.mapEntryPoint(
        {.key = top, .x = 960, .y = 1079},
        mks::Edge::Bottom
    );
    ASSERT_TRUE(mapped.has_value()) << mapped.error().message();
    EXPECT_EQ(mapped->key, bottom);
    EXPECT_EQ(mapped->x, 640);
    EXPECT_EQ(mapped->y, 0);
}

TEST(ScreenTopology, ReportsMissingNeighborWhenMappingAcrossEmptyCell) {
    auto topology = mks::ScreenTopology {};
    const auto key = makeKey("screen");
    ASSERT_TRUE(topology.addScreen(makeScreen(key, {0, 0}, 1920, 1080)).has_value());

    auto mapped = topology.mapEntryPoint(
        {.key = key, .x = 1919, .y = 500},
        mks::Edge::Right
    );
    ASSERT_FALSE(mapped.has_value());
    EXPECT_EQ(
        mapped.error(),
        mks::make_error_code(mks::TopologyError::MissingNeighbor)
    );
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
