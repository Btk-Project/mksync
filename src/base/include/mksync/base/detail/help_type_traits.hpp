/**
 * @file help_type_traits.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-03-14
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once
#include <mksync/base/base_library.h>

#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include <rapidjson/document.h>
#include <nekoproto/proto/serializer_base.hpp>
#include <nekoproto/proto/json_serializer.hpp>

MKS_BEGIN

namespace detail
{
    template <typename T>
    struct IsVariant : std::false_type {};

    template <typename... Types>
    struct IsVariant<std::variant<Types...>> : std::true_type {};

    template <typename T>
    constexpr bool IsVariantV = IsVariant<std::remove_cvref_t<T>>::value;

    template <typename Allocator>
    rapidjson::Value to_json_value(bool value, [[maybe_unused]] Allocator &allocator)
    {
        return value ? rapidjson::Value(rapidjson::kTrueType)
                     : rapidjson::Value(rapidjson::kFalseType);
    }

    template <typename T, typename Allocator>
        requires(std::is_arithmetic_v<T> && !std::is_same_v<T, bool>)
    rapidjson::Value to_json_value(const T &value, [[maybe_unused]] Allocator &allocator)
    {
        rapidjson::Value val(rapidjson::kNumberType);
        if constexpr (std::is_signed_v<T> && sizeof(T) < sizeof(int64_t)) {
            val.SetInt(value);
        }
        else if constexpr (std::is_signed_v<T> && sizeof(T) >= sizeof(int64_t)) {
            val.SetInt64(value);
        }
        else if constexpr (std::is_unsigned_v<T> && sizeof(T) < sizeof(uint64_t)) {
            val.SetUint(value);
        }
        else if constexpr (std::is_unsigned_v<T> && sizeof(T) >= sizeof(uint64_t)) {
            val.SetUint64(value);
        }
        else if constexpr (std::is_floating_point_v<T> && sizeof(T) < sizeof(double)) {
            val.SetFloat(value);
        }
        else if constexpr (std::is_floating_point_v<T> && sizeof(T) >= sizeof(double)) {
            val.SetDouble(value);
        }
        else {
            std::string str = std::to_string(value);
            val.SetString(str.c_str(), str.length());
        }
        return val;
    }

    template <typename T, typename Allocator>
        requires std::convertible_to<T, std::string> || std::constructible_from<std::string, T>
    rapidjson::Value to_json_value(const T &value, Allocator &allocator)
    {
        std::string str(value);
        return rapidjson::Value(str.c_str(), (int)str.length(), allocator);
    }

    template <typename T, typename Allocator>
        requires std::is_enum_v<T>
    rapidjson::Value to_json_value(const T &value, Allocator &allocator)
    {
        return to_json_value(static_cast<std::underlying_type_t<T>>(value), allocator);
    }

    template <typename T, typename Allocator>
        requires requires(T va) {
            {
                detail::to_json_value(*std::begin(va),
                                      std::declval<RAPIDJSON_DEFAULT_ALLOCATOR &>())
            };
            { std::end(va) };
        } && (!std::is_constructible_v<std::string, T>) && (!std::is_convertible_v<T, std::string>)
    rapidjson::Value to_json_value(const T &value, Allocator &allocator)
    {
        rapidjson::Value arr(rapidjson::kArrayType);
        for (const auto &val : value) {
            arr.PushBack(detail::to_json_value(val, allocator), allocator);
        }
        return arr;
    }

    template <typename... Ts>
    struct unfold_option_variant_to_json_helper { // NOLINT(readability-identifier-naming)
        template <typename T, std::size_t N, typename Allocator>
        static std::string unfold_value_imp2(const std::variant<Ts...> &value, Allocator *allocator)
        {
            if (value.index() != N) {
                return "";
            }
            return to_json_value(std::get<N>(value), allocator);
        }

        template <std::size_t... Ns, typename Allocator>
        static std::string unfold_value(const std::variant<Ts...> &value, Allocator &allocator,
                                        std::index_sequence<Ns...> /*unused*/)
        {
            return (unfold_value_imp2<std::variant_alternative_t<Ns, std::variant<Ts...>>, Ns>(
                        value, allocator) +
                    ...);
        }
    };

    template <typename... Ts, typename Allocator>
    std::string to_json_value(const std::variant<Ts...> &value, Allocator &allocator)
    {
        return unfold_option_variant_to_json_helper<Ts...>::unfold_value(
            value, allocator, std::index_sequence_for<Ts...>{});
    }

    template <typename T>
        requires std::is_same_v<T, bool>
    bool from_json_value(const rapidjson::Value &value, T &result)
    {
        if (value.IsBool()) {
            result = value.GetBool();
            return true;
        }
        return false;
    }

    template <typename T>
        requires(std::is_arithmetic_v<T> && !std::is_same_v<T, bool>)
    bool from_json_value(const rapidjson::Value &value, T &result)
    {
        if (value.IsUint64()) {
            result = (T)value.GetUint64();
            return true;
        }
        if (value.IsUint()) {
            result = (T)value.GetUint();
            return true;
        }
        if (value.IsInt64()) {
            result = (T)value.GetInt64();
            return true;
        }
        if (value.IsInt()) {
            result = (T)value.GetInt();
            return true;
        }
        if (value.IsFloat()) {
            result = (T)value.GetFloat();
            return true;
        }
        if (value.IsDouble()) {
            result = (T)value.GetDouble();
            return true;
        }
        return false;
    }

    // TODO: support proto
    template <typename T>
        requires std::is_convertible_v<std::string, T>
    bool from_json_value(const rapidjson::Value &value, T &result)
    {
        if (value.IsString()) {
            result = T(value.GetString(), value.GetStringLength());
            return true;
        }
        return false;
    }

    template <typename T>
        requires std::is_enum_v<T>
    bool from_json_value(const rapidjson::Value &value, T &result)
    {
        std::underlying_type_t<T> res;
        if (from_json_value(value, res)) {
            result = static_cast<T>(res);
            return true;
        }
        return false;
    }

    template <typename T>
    bool from_json_value(const rapidjson::Value &value, std::vector<T> &result)
    {
        if (value.IsArray()) {
            result.resize(value.Size());
            for (rapidjson::SizeType i = 0; i < value.Size(); i++) {
                if (!from_json_value(value[i], result[i])) {
                    result.clear();
                    return false;
                }
            }
            return true;
        }
        return false;
    }
} // namespace detail

MKS_END