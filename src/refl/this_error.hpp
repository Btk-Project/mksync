/**
 * @file this_error.hpp
 * @author BusyStudent (fyw90mc@gmail.com)
 * @brief Helper macros to generate the error code for enum type.
 * @version 0.1
 * @date 2026-05-25
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#pragma once

#include <system_error> // std::error_code
#include <type_traits>
#include <string>
#include <format>
#include <print>
#include <meta>

/**
 * @brief Declare a enum is a error code type
 * 
 */
#define THIS_ERROR(ENUM)                                                          \
    static_assert(std::is_enum_v<ENUM>, "ENUM must be an enum type");             \
    extern auto _this_error_get_category_##ENUM() -> const std::error_category &; \
    extern auto _this_error_message(ENUM e) -> std::string_view;                  \
    inline auto _this_error_make_code(ENUM e) -> std::error_code {                \
        return { static_cast<int>(e), _this_error_get_category_##ENUM() };        \
    }

/**
 * @brief Generate the implementation of THIS_ERROR (put it in .cpp file)
 * @param ENUM The enum type, at the same namespace 
 * 
 */
#define THIS_ERROR_IMPL(ENUM)                                                       \
    static_assert(ThisError<ENUM>, "ENUM must be declarated with THIS_ERROR(ENUM)");\
    auto _this_error_get_category_##ENUM() -> const std::error_category & {         \
        struct Category : public std::error_category {                              \
            auto name() const noexcept -> const char * override { return #ENUM; }   \
            auto message(int c) const noexcept -> std::string override {            \
                auto e = static_cast<ENUM>(c);                                      \
                return std::string {_this_error_message(e)};                        \
            }                                                                       \
        };                                                                          \
        static constinit Category c {};                                             \
        return c;                                                                   \
    }                                                                               \
    auto _this_error_message(ENUM e) -> std::string_view {                          \
        return ::this_error::detail::enumToString(e);                               \
    }

// Add an error message to the error code
// Use this macro to make LSP happy :(
#if defined(__cpp_lib_reflection)
    #define THIS_ERROR_MESSAGE(str) [[=this_error::detail::Message{std::define_static_string(str)}]]
#else
    #define THIS_ERROR_MESSAGE(str) 
#endif // __cpp_reflection

// ADL Lookup
template <typename T>
concept ThisError = requires(T t) {
    { _this_error_make_code(t) } -> std::convertible_to<std::error_code>;
};

// ADL Bridge for std::error_code
template <ThisError T>
inline auto make_error_code(T e) -> std::error_code {
    return _this_error_make_code(e);
}

// Enable ErrorCode for enum types
template <ThisError T>
struct std::is_error_code_enum<T> : std::true_type {};

// Enable Format for enum types
template <ThisError T>
struct std::formatter<T> {
    std::formatter<std::string_view> inner;

    constexpr auto parse(std::format_parse_context &ctxt) -> decltype(ctxt.begin()) {
        return inner.parse(ctxt);
    }

    template <typename FormatContext>
    auto format(T e, FormatContext &ctxt) const -> decltype(ctxt.out()) {
        return inner.format(_this_error_message(e), ctxt);
    }
};

template <ThisError T>
inline constexpr bool std::enable_nonlocking_formatter_optimization<T> = true;

// Detail for this_error
namespace this_error::detail {

// Helper struct to annotation
struct Message {
    const char *text;
};

consteval auto annotationOf(auto meta) -> std::string_view {
    auto annotations = std::meta::annotations_of_with_type(meta, ^^Message);
    if (annotations.empty()) {
        return {};
    }
    else {
        return std::meta::extract<Message>(annotations[0]).text;
    }
}

template <ThisError T>
inline auto enumToString(T value) -> std::string_view {
    if constexpr (std::meta::is_enumerable_type(^^T)) {
        template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^T))) {
            if (value != [:e:]) {
                continue;
            }
            // Check annotations
            constexpr auto annotation = annotationOf(e);
            if constexpr(annotation.empty()) {
                return std::meta::identifier_of(e);             
            }
            else {
                return annotation;
            }
        }
    }
    return "<unknown>";
}

} // namespace this_error::detail