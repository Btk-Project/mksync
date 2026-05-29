/**
 * @file serde.hpp
 * @author BusyStudent (fyw90mc@gmail.com)
 * @brief Serialization and deserialization by using C++ reflection
 * @version 0.1
 * @date 2026-05-28
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#pragma once

#include <type_traits>
#include <expected>
#include <utility>
#include <string>
#include <ranges>

template <typename T>
concept Serializer = requires(T t) {
    // Primitive types
    t.serializeFloat();
    t.serializeInt();
    t.serializeBool();

    // String
    t.serializeString();

    // Array
    t.serializeArray();
};

namespace serde {

} // namespace serde