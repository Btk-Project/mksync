#include "refl/this_error.hpp"
#include "refl/formatter.hpp"
#include <gtest/gtest.h>
#include <string>

TEST(Refl, Enum) {
    enum Hello {
        A, B, C
    };

    EXPECT_EQ(refl::detail::enumToString(Hello::A), "A");
    EXPECT_EQ(refl::detail::enumToString(Hello::B), "B");
    EXPECT_EQ(refl::detail::enumToString(Hello::C), "C");

    EXPECT_EQ(refl::detail::enumFromString<Hello>("A"), Hello::A);
    EXPECT_EQ(refl::detail::enumFromString<Hello>("B"), Hello::B);
    EXPECT_EQ(refl::detail::enumFromString<Hello>("C"), Hello::C);
    EXPECT_EQ(refl::detail::enumFromString<Hello>("D"), std::nullopt);
}

enum class TestError {
    Ok = 0,
    Test = 1,
    MyMessage THIS_ERROR_MESSAGE("This is my message") = 10,
};
THIS_ERROR(TestError);
THIS_ERROR_IMPL(TestError);

enum class FormatterKind {
    First,
    Second,
};
FORMATTER(FormatterKind);

struct FormatterValue {
    int           count;
    std::string   name;
    FormatterKind kind;
};
FORMATTER(FormatterValue);

TEST(Refl, ThisError) {
    std::error_code ec = TestError::Test;
    EXPECT_EQ(ec.message(), "Test");
    ec = TestError::Ok;
    EXPECT_EQ(ec.message(), "Ok");
    ec = TestError::MyMessage;
    EXPECT_EQ(ec.message(), "This is my message");

    // Test format
    EXPECT_EQ(std::format("{}", TestError::Test), "Test");
    EXPECT_EQ(std::format("{}", TestError::MyMessage), "This is my message");
}

TEST(Refl, Formatter) {
    EXPECT_EQ(std::format("Hello {}!", TestError::Test), "Hello Test!");
    EXPECT_EQ(std::format("{}", FormatterKind::Second), "Second");
    EXPECT_EQ(std::format("{}", FormatterValue { .count = 7, .name = "main", .kind = FormatterKind::First}), "FormatterValue { count: 7, name: main, kind: First }");
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}