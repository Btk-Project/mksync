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

#include "../config/config.hpp"

#include <type_traits>
#include <expected>
#include <utility>
#include <string>
#include <ranges>
#include <bit>
#include <nekoproto/serialization/json_serializer.hpp>
#include <nekoproto/serialization/parsing/parsers.hpp>

MKS_BEGIN
#if defined(NEKO_PROTO_ENABLE_RAPIDJSON)
using SerializerBuffer = ::NekoProto::detail::PrettyJsonWriter<std::vector<char>>;
using Serializer = ::NekoProto::RapidJsonOutputSerializer<SerializerBuffer>;
using Deserializer = ::NekoProto::JsonSerializer::InputSerializer;
using JsonOutputFormatOptions = ::NekoProto::JsonOutputFormatOptions;
#elif defined(NEKO_PROTO_ENABLE_SIMDJSON)
using Serializer = ::NekoProto::JsonSerializer::OutputSerializer;
using Deserializer = ::NekoProto::JsonSerializer::InputSerializer;
#else
#error "No json serializer is enabled"
#endif
MKS_END