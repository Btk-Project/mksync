#include "refl/this_error.hpp"
#include "refl/formatter.hpp"
#include "refl/enum.hpp"
#include <gtest/gtest.h>

namespace refl = mks::refl;

TEST(Refl, Enum) {
    enum Hello {
        A, B, C
    };

    EXPECT_EQ(refl::enumToString(Hello::A), "A");
    EXPECT_EQ(refl::enumToString(Hello::B), "B");
    EXPECT_EQ(refl::enumToString(Hello::C), "C");

    EXPECT_EQ(refl::enumFromString<Hello>("A"), Hello::A);
    EXPECT_EQ(refl::enumFromString<Hello>("B"), Hello::B);
    EXPECT_EQ(refl::enumFromString<Hello>("C"), Hello::C);
    EXPECT_EQ(refl::enumFromString<Hello>("D"), std::nullopt);
}

enum class TestError {
    Ok = 0,
    Test = 1,
};
THIS_ERROR(TestError);
THIS_ERROR_IMPL(TestError);

ENUM_FORMATTER(TestError);
ENUM_FORMATTER_IMPL(TestError);

TEST(Refl, ThisError) {
    std::error_code ec = TestError::Test;
    EXPECT_EQ(ec.message(), "Test");
}

TEST(Refl, Formatter) {
    EXPECT_EQ(std::format("Hello {}!", TestError::Test), "Hello Test!");
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}