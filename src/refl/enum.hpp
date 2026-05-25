/**
 * @file enuim.hpp
 * @author BusyStudent (fyw90mc@gmail.com)
 * @brief Reflection for enum
 * @version 0.1
 * @date 2026-05-25
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#pragma once

#include "config/config.hpp"
#include <type_traits>
#include <optional>
#include <format>
#include <meta>

MKS_BEGIN

namespace refl {

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
 * @brief Convert a enum flags to string
 * 
 * @tparam T 
 */
template <typename T> requires (std::is_enum_v<T>)
inline auto flagsToString(T value) -> std::string {
    std::string out;
    if constexpr (std::meta::is_enumerable_type(^^T)) {
        template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^T))) {
            if (static_cast<bool>(value & [:e:])) {
                out += std::meta::identifier_of(e);
                out += " | ";
            }
        }
    }
    if (out.empty()) {
        out = "<empty>";
    }
    else { // Remove last " | "
        out.pop_back();
        out.pop_back();
        out.pop_back();
    }
    return out;
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

} // namespace refl

MKS_END