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

#include "config/config.hpp"
#include "enum.hpp"
#include <system_error> // std::error_code
#include <type_traits>

/**
 * @brief Declare a enum is a error code type
 * 
 */
#define THIS_ERROR(ENUM)                                                          \
    static_assert(std::is_enum_v<ENUM>, "ENUM must be an enum type");             \
    extern auto _this_error_get_category_##ENUM() -> const std::error_category &; \
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
                return std::string {                                                \
                    mks::refl::enumToString(e)                                      \
                };                                                                  \
            }                                                                       \
        };                                                                          \
        static constinit Category c {};                                             \
        return c;                                                                   \
    }

// Add an error message to the error code
#if defined(__cpp_reflection)
    #define THIS_ERROR_MESSAGE(str) [[=str]]
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