#pragma once

#include "config/config.hpp"
#include <type_traits>
#include <concepts>
#include <format>
#include <string>

// Formatter for enums
#define ENUM_FORMATTER(ENUM) \
    extern auto _refl_to_string(ENUM value) -> std::string_view

#define ENUM_FORMATTER_IMPL(ENUM) \
    static_assert(std::formattable<ENUM, char>, "ENUM must be formattable"); \
    auto _refl_to_string(ENUM value) -> std::string_view {                   \
        return ::mks::refl::enumToString(value);                             \
    }                                                             

// Formatter for enum flags
#define FLAGS_FORMATTER(ENUM) \
    extern auto _refl_to_string(ENUM value) -> std::string

#define FLAGS_FORMATTER_IMPL(ENUM) \
    static_assert(std::formattable<ENUM, char>, "ENUM must be formattable"); \
    auto _refl_to_string(ENUM value) -> std::string {                        \
        return ::mks::refl::flagsToString(value);                            \
    }                                                             

// Formatter for structs
#define STRUCT_FORMATTER(STRUCT) \
    extern auto _refl_to_string(const STRUCT &value) -> std::string

#define STRUCT_FORMATTER_IMPL(STRUCT) \
    static_assert(std::formattable<STRUCT, char>, "STRUCT must be formattable"); \

template <typename T>
concept Formattable = requires(T t) {
    { _refl_to_string(t) } -> std::convertible_to<std::string_view>;
};

// Bridges to reflection formatter
template <Formattable T>
struct std::formatter<T> {
    constexpr auto parse(format_parse_context& ctx) const -> decltype(ctx.begin()) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const T& t, FormatContext& ctx) const -> decltype(ctx.out()) {
        return format_to(ctx.out(), "{}", _refl_to_string(t));
    }
};