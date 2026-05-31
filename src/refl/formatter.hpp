/**
 * @file formatter.hpp
 * @author BusyStudent (fyw90mc@gmail.com)
 * @brief Generic formatter for enum and struct
 * @version 0.1
 * @date 2026-05-28
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#pragma once

#include <type_traits>
#include <concepts>
#include <variant>
#include <string>
#include <format>
#include <print>
#include <meta>

// MARK: Inline
// Generic Formatter for enums and structs/classes
#define FORMATTER(TYPE)                                          \
    inline auto _refl_fmt_inline(const TYPE &value, auto it) {   \
        return ::refl::detail::formatTo(value, it);              \
    }                                                            \

// Formatter for enum flags
#define FLAGS_FORMATTER(ENUM)                            \
    inline auto _refl_fmt_inline(ENUM value, auto it) {  \
        return ::refl::detail::formatFlags(value, it);   \
    }                                                             

// Formatter for variant
#define VARIANT_FORMATTER(TYPE)                                 \
    template <char = 0>                                         \
    inline auto _refl_fmt_inline(const TYPE &value, auto it) {  \
        return ::refl::detail::formatVariant(value, it);        \
    }

// MARK: Extern
#define EXTERN_FORMATTER(TYPE) \
    extern auto _refl_fmt_extern(const TYPE &value) -> std::string;

// Temp disable the impl
#define FORMATTER_IMPL(TYPE)
#define FLAGS_FORMATTER_IMPL(ENUM)

// Check an type is mark by macro
template <typename T>
concept Formattable = requires(T t) {
    _refl_fmt_inline(t, std::declval<std::back_insert_iterator<std::string> >());
} || requires(T t) {
    _refl_fmt_extern(t);   
};

// Bridges to reflection formatter
template <Formattable T>
struct std::formatter<T> {
    constexpr auto parse(std::format_parse_context &ctxt) const -> decltype(ctxt.begin()) {
        return ctxt.begin();
    }

    template <typename FormatContext>
    auto format(const T &t, FormatContext &ctxt) const -> decltype(ctxt.out()) {
        if constexpr (requires { _refl_fmt_inline(t, ctxt.out()); }) { // Inline formatter
            return _refl_fmt_inline(t, ctxt.out());
        }
        else {
            static_assert(false, "Extern formatter not implemented now"); // 
        }
    }
};

template <Formattable T>
inline constexpr bool std::enable_nonlocking_formatter_optimization<T> = true;

// Detail code for struct formatter
namespace refl::detail {

/**
 * @brief Convert enum to string
 * 
 * @tparam T 
 */
template <typename T> requires (std::is_enum_v<T>)
inline auto enumToString(T value) -> std::string_view {
    if constexpr (std::meta::is_enumerable_type(^^T)) {
        template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^T))) {
            if (value == [:e:]) {
                return std::meta::identifier_of(e);
            }
        }
    }
    return "<unknown>";
}

/**
 * @brief Convert string to enum
 * 
 * @tparam T 
 */
template <typename T> requires (std::is_enum_v<T>)
inline auto enumFromString(std::string_view name) -> std::optional<T> {
    if constexpr (std::meta::is_enumerable_type(^^T)) {
        template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^T))) {
            if (name == std::meta::identifier_of(e)) {
                return [:e:];
            }
        }
    }

    return std::nullopt;
}

/**
 * @brief Format an enum to string
 * 
 * @tparam T 
 * @tparam Context
 */
template <typename T> requires (std::is_enum_v<T>)
inline auto formatFlags(T value, auto it) {
    auto prev = false;
    if constexpr (std::meta::is_enumerable_type(^^T)) {
        template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^T))) {
            if (static_cast<bool>(value & [:e:])) {
                if (prev) {
                    it = std::format_to(it, " | "); // " | "
                }
                it = std::format_to(it, "{}", std::meta::identifier_of(e));
                prev = true;
            }
        }
    }
    if (!prev) {
        it = std::format_to(it, "<none>");
    }
    return it;
}

template <typename Raw>
inline auto formatVariant(const Raw &value, auto it) {
    using T = std::remove_cvref_t<Raw>;
    return std::visit([&](const auto &element) { // TypeName { InnerType }
        return std::format_to(
            it,
            "{} {{ {} }}",
            std::meta::identifier_of(std::meta::dealias(^^T)),
            element
        );
    }, value);
}

/**
 * @brief Convert struct to string
 * 
 * @tparam T 
 */
template <typename T> requires(std::is_class_v<T>)
inline auto formatStruct(const T &value, auto it) {
    auto prev = false;

    // StructName { elem: value... }
    it = std::format_to(it, "{} {{ ", std::meta::identifier_of(^^T));
    template for (constexpr auto member : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current()))) {
        if (prev) {
            it = std::format_to(it, ", ");
        }
        prev = true;
        if constexpr (std::formattable<decltype(value.[:member:]), char>) {
            it = std::format_to(it, "{}: {}", std::meta::identifier_of(member), value.[:member:]);
        }
        else {
            it = std::format_to(it, "{}: <unformattable>", std::meta::identifier_of(member));
        }
    }

    it = std::format_to(it, " }}");
    return it;
}

// Use overloads to dispatch
template <typename T> requires(std::is_enum_v<T>)
inline auto formatTo(const T &value, auto it) {
    return std::format_to(it, "{}", enumToString(value));
}

template <typename T> requires(std::is_class_v<T>)
inline auto formatTo(const T &value, auto it) {
    return formatStruct(value, it);
}

} // namespace refl::detail