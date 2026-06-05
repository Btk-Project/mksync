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
#include <bit>
#include <nekoproto/serialization/json_serializer.hpp>
#include <nekoproto/serialization/types/types.hpp>

MKS_BEGIN

using Serializer = ::NekoProto::JsonSerializer::OutputSerializer;
using Deserializer = ::NekoProto::JsonSerializer::InputSerializer;

MKS_END