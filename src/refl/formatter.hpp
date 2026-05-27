#pragma once

#include "config/config.hpp"
#include "refl/enum.hpp"
#include <concepts>
#include <format>
#include <string_view>
#include <string>
#include <type_traits>
#include <meta>

// Generic Formatter for enums and structs/classes
#define FORMATTER(TYPE) extern auto _refl_to_string(const TYPE &value) -> std::string

#define FORMATTER_IMPL(TYPE)                                                               \
    static_assert(std::formattable<TYPE, char>, "TYPE must be formattable");               \
    auto _refl_to_string(const TYPE &value) -> std::string {                               \
        return ::mks::refl::formatToString(value);                                         \
    }                                                                                      \

    // Formatter for enum flags
#define FLAGS_FORMATTER(ENUM) \
    extern auto _refl_to_string(ENUM value) -> std::string

#define FLAGS_FORMATTER_IMPL(ENUM) \
    static_assert(std::formattable<ENUM, char>, "ENUM must be formattable"); \
    auto _refl_to_string(ENUM value) -> std::string {                        \
        return ::mks::refl::flagsToString(value);                            \
    }                                                             

// Check an type is mark by macro
template <typename T>
concept Formattable = requires(T t) {
    _refl_to_string(t);
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

// Detail code for struct formatter
MKS_BEGIN

namespace refl {

/**
 * @brief Convert struct to string
 * 
 * @tparam T 
 */
template <typename T> requires(std::is_class_v<T>)
inline auto structToString(const T &value) -> std::string {
    auto out = std::string {std::meta::identifier_of(^^T)};
    auto first = true;

    out += " { ";
    template for (constexpr auto member : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current()))) {
        if (!first) {
            out += ", ";
        }
        first = false;
        if constexpr (std::formattable<decltype(value.[:member:]), char>) {
            std::format_to(std::back_inserter(out), "{}: {}", std::meta::identifier_of(member), value.[:member:]);
        }
        else {
            std::format_to(std::back_inserter(out), "{}: <unformattavble>", std::meta::identifier_of(member));
        }
    }

    out += " }";
    return out;
}

template <typename T>
inline auto formatToString(const T &value) -> std::string
{
    using U = std::remove_cvref_t<T>;

    if constexpr (std::is_enum_v<U>) {
        return std::string {enumToString(value)};
    }
    else if constexpr (std::is_class_v<U>) {
        return structToString(value);
    }
    else {
        static_assert(std::is_enum_v<U> || std::is_class_v<U>, "FORMATTER only supports enum and struct/class types");
    }
}

} // namespace refl

MKS_END