#include "core/key.hpp"
#include "refl/this_error.hpp"
#include "refl/formatter.hpp"
#include <gtest/gtest.h>
#include <string>

TEST(Refl, Enum) {
    enum Hello {
        A, B, C, HelloMax = 255
    };

    EXPECT_EQ(refl::detail::enumToString(Hello::A), "A");
    EXPECT_EQ(refl::detail::enumToString(Hello::B), "B");
    EXPECT_EQ(refl::detail::enumToString(Hello::C), "C");

    EXPECT_EQ(refl::detail::enumFromString<Hello>("A"), Hello::A);
    EXPECT_EQ(refl::detail::enumFromString<Hello>("B"), Hello::B);
    EXPECT_EQ(refl::detail::enumFromString<Hello>("C"), Hello::C);
    EXPECT_EQ(refl::detail::enumFromString<Hello>("D"), std::nullopt);
}

TEST(Refl, KeyEnum) {
    EXPECT_EQ(refl::detail::enumToString(mks::Key::None), "None");
    EXPECT_EQ(refl::detail::enumToString(mks::Key::A), "A");
    EXPECT_EQ(refl::detail::enumToString(mks::Key::F24), "F24");
    EXPECT_EQ(refl::detail::enumToString(mks::Key::RightMeta), "RightMeta");
    EXPECT_EQ(refl::detail::enumToString(mks::Key::MediaCalc), "MediaCalc");

    EXPECT_EQ(refl::detail::enumFromString<mks::Key>("None"), mks::Key::None);
    EXPECT_EQ(refl::detail::enumFromString<mks::Key>("A"), mks::Key::A);
    EXPECT_EQ(refl::detail::enumFromString<mks::Key>("F24"), mks::Key::F24);
    EXPECT_EQ(refl::detail::enumFromString<mks::Key>("RightMeta"), mks::Key::RightMeta);
    EXPECT_EQ(refl::detail::enumFromString<mks::Key>("MediaCalc"), mks::Key::MediaCalc);
    EXPECT_EQ(refl::detail::enumFromString<mks::Key>("NotAKey"), std::nullopt);

    EXPECT_EQ(fmtlib::format("{}", mks::Key::MediaCalc), "MediaCalc");
}

TEST(Refl, KeyModifierEnum) {
    EXPECT_EQ(refl::detail::enumToString(mks::KeyModifier::None), "None");
    EXPECT_EQ(refl::detail::enumToString(mks::KeyModifier::LeftCtrl), "LeftCtrl");
    EXPECT_EQ(refl::detail::enumToString(mks::KeyModifier::RightMeta), "RightMeta");

    EXPECT_EQ(
        refl::detail::enumFromString<mks::KeyModifier>("LeftCtrl"),
        mks::KeyModifier::LeftCtrl
    );
    EXPECT_EQ(
        refl::detail::enumFromString<mks::KeyModifier>("RightMeta"),
        mks::KeyModifier::RightMeta
    );
    EXPECT_EQ(refl::detail::enumFromString<mks::KeyModifier>("MiddleCtrl"), std::nullopt);

    EXPECT_EQ(fmtlib::format("{}", mks::KeyModifier::None), "<none>");
    EXPECT_EQ(
        fmtlib::format("{}", mks::KeyModifier::LeftCtrl | mks::KeyModifier::RightAlt),
        "LeftCtrl | RightAlt"
    );
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
#ifdef NO_THIS_ERROR_MESSAGE
    EXPECT_EQ(ec.message(), "MyMessage");
    EXPECT_EQ(fmtlib::format("{}", TestError::MyMessage), "MyMessage");
#else
    EXPECT_EQ(ec.message(), "This is my message");
    EXPECT_EQ(fmtlib::format("{}", TestError::MyMessage), "This is my message");
#endif
    // Test format
    EXPECT_EQ(fmtlib::format("{}", TestError::Test), "Test");
}

TEST(Refl, Formatter) {
    EXPECT_EQ(fmtlib::format("Hello {}!", TestError::Test), "Hello Test!");
    EXPECT_EQ(fmtlib::format("{}", FormatterKind::Second), "Second");
    EXPECT_EQ(fmtlib::format("{}", FormatterValue { .count = 7, .name = "main", .kind = FormatterKind::First}), "FormatterValue { count: 7, name: main, kind: First }");
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
