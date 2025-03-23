#include <gtest/gtest.h>

#include "mksync/base/ring_buffer.hpp"

using namespace mks::base;

TEST(RingBuffer, Test)
{
    RingBuffer<int> buffer(5);
    EXPECT_EQ(buffer.size(), 0);
    EXPECT_EQ(buffer.capacity(), 5);

    buffer.push(1);
    EXPECT_EQ(buffer.size(), 1);
    buffer.push(2);
    EXPECT_EQ(buffer.size(), 2);
    buffer.push(3);
    EXPECT_EQ(buffer.size(), 3);
    buffer.push(4);
    EXPECT_EQ(buffer.size(), 4);
    buffer.push(5);
    EXPECT_EQ(buffer.size(), 5);
    buffer.push(6);
    EXPECT_EQ(buffer.size(), 5);
    int value;
    EXPECT_TRUE(buffer.pop(value));
    EXPECT_EQ(value, 2);
    EXPECT_TRUE(buffer.pop(value));
    EXPECT_EQ(value, 3);
    EXPECT_TRUE(buffer.pop(value));
    EXPECT_EQ(value, 4);
    EXPECT_TRUE(buffer.pop(value));
    EXPECT_EQ(value, 5);
    EXPECT_TRUE(buffer.pop(value));
    EXPECT_EQ(value, 6);
    EXPECT_FALSE(buffer.pop(value));
    EXPECT_EQ(buffer.size(), 0);
    EXPECT_EQ(buffer.capacity(), 5);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}