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
#include <iterator>
#include <variant>
#include <string>

#if __cpp_lib_format >= 202207L
#define MKS_USE_STD_FORMAT 1
#include <format>
namespace fmtlib = std;
#else
#define MKS_USE_STD_FORMAT 0
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
namespace fmtlib = fmt;
#if FMT_VERSION < 110000
namespace fmt {
    template<typename T, typename Char = char>
    concept formattable = requires (T&& t, fmt::format_context ctx) {
        fmt::formatter<std::remove_cvref_t<T>, Char>().format(t, ctx);
    };
}
#endif // FMT_VERSION < 110000
#endif // __cpp_lib_format >= 202207L
#include <print>
#include <map>

#define NEKO_ENUM_SEARCH_DEPTH 256
#include <nekoproto/serialization/reflection.hpp>

// {fmt} supports user-defined types through an ADL-found format_as overload. Map reflected
// values to a project-owned view so formatting stays allocation-free and its implementation is
// instantiated only where the value is actually formatted. The delayed instantiation matters
// for flags whose operators can be declared later in the same header.
#if MKS_USE_STD_FORMAT
#define REFL_FORMAT_AS(TYPE)
#else
namespace refl {
template <typename T>
struct FormatView {
    const T &value;
};
}

template <typename T, typename Char>
struct fmt::formatter<::refl::FormatView<T>, Char> {
    constexpr auto parse(fmt::basic_format_parse_context<Char> &context) const
        -> decltype(context.begin()) {
        return context.begin();
    }

    template <typename FormatContext>
    auto format(const ::refl::FormatView<T> &view, FormatContext &context) const
        -> decltype(context.out()) {
        return _refl_fmt_inline(view.value, context.out());
    }
};

#define REFL_FORMAT_AS(TYPE)                                               \
    inline auto format_as(const TYPE &value) -> ::refl::FormatView<TYPE> { \
        return {value};                                                    \
    }
#endif

// MARK: Inline
// Generic Formatter for enums and structs/classes
#define FORMATTER(TYPE)                                           \
    inline auto _refl_fmt_inline(const TYPE &value, auto it) {    \
        return ::refl::detail::formatTo(value, it);               \
    }                                                             \
    REFL_FORMAT_AS(TYPE)

// Formatter for enum flags
#define FLAGS_FORMATTER(ENUM)                                     \
    inline auto _refl_fmt_inline(ENUM value, auto it) {           \
        return ::refl::detail::formatFlags(value, it);            \
    }                                                             \
    REFL_FORMAT_AS(ENUM)

// Formatter for variant
#define VARIANT_FORMATTER(TYPE)                                   \
    template <char = 0>                                           \
    inline auto _refl_fmt_inline(const TYPE &value, auto it) {    \
        return ::refl::detail::formatVariant(#TYPE, value, it);   \
    }                                                             \
    REFL_FORMAT_AS(TYPE)

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

// std::format has no format_as extension point, so retain the formatter bridge for that path.
#if MKS_USE_STD_FORMAT
template <Formattable T>
struct fmtlib::formatter<T> {
    constexpr auto parse(fmtlib::format_parse_context &ctxt) const -> decltype(ctxt.begin()) {
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
#endif

#if defined(__cpp_lib_print) && __cpp_lib_print >= 202406L
template <Formattable T>
inline constexpr bool std::enable_nonlocking_formatter_optimization<T> = true;
#endif

// Detail code for struct formatter
namespace refl::detail {

/**
 * @brief Convert enum to string
 * 
 * @tparam T 
 */
template <typename T> requires (std::is_enum_v<T>)
inline auto enumToString(T value) -> std::string_view {
    auto name = ::NekoProto::Reflect<T>::name(value);
    if (name != "") {
        auto pos = name.find_last_of(':');
        if (pos != std::string_view::npos) {
            return name.substr(pos + 1);
        }
        return name;
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
    auto &namesMap = ::NekoProto::Reflect<T>::nameMap();
    if (namesMap.empty()) {
        return std::nullopt;
    }
    auto nameWithNs = std::string {name};
    auto begin = namesMap.begin()->first;
    // get namespace
    auto pos = begin.find_last_of(':');
    if (pos != std::string_view::npos) {
        begin = begin.substr(0, pos + 1);
        if (name.find(begin) == std::string_view::npos) {
            nameWithNs = std::string(begin) + nameWithNs;
        }
    }
    auto value = namesMap.find(nameWithNs);
    if (value != namesMap.end()) {
        return value->second;
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
    auto values = ::NekoProto::Reflect<T>::values();
    for (const auto &e : values) {
        if (static_cast<bool>(value & e)) {
            if (prev) {
                it = fmtlib::format_to(it, " | "); // " | "
            }
            it = fmtlib::format_to(it, "{}", enumToString(e));
            prev = true;
        }
    }
    if (!prev) {
        it = fmtlib::format_to(it, "<none>");
    }
    return it;
}

template <typename Raw>
inline auto formatVariant(std::string_view name, const Raw &value, auto it) {
    return std::visit([&](const auto &element) { // TypeName { InnerType }
        return fmtlib::format_to(
            it,
            "{} {{ {} }}",
            name,
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
    if constexpr (::NekoProto::detail::member_count_v<T> == 0) {
        it = fmtlib::format_to(it, "{} {{ }}", ::NekoProto::Reflect<T>::className());
    }
    else {
        it = fmtlib::format_to(it, "{} {{ ", ::NekoProto::Reflect<T>::className());
        ::NekoProto::Reflect<T>::forEach(value, [&](const auto& member, std::string_view name) {
            if (prev) {
                it = fmtlib::format_to(it, ", ");
            }
            prev = true;
            if constexpr (fmtlib::formattable<decltype(member), char>) {
                it = fmtlib::format_to(it, "{}: {}", name == "" ? "<unnamed>" : name, member);
            }
            else {
                it = fmtlib::format_to(it, "{}: <unformattable>", name == "" ? "<unnamed>" : name);
            }
        });
    }

    it = fmtlib::format_to(it, " }}");
    return it;
}

// Use overloads to dispatch
template <typename T> requires(std::is_enum_v<T>)
inline auto formatTo(const T &value, auto it) {
    return fmtlib::format_to(it, "{}", enumToString(value));
}

template <typename T> requires(std::is_class_v<T>)
inline auto formatTo(const T &value, auto it) {
    return formatStruct(value, it);
}

} // namespace refl::detail
