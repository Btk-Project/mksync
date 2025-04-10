/**
 * @file setting_serializer_help.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-03-29
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once
#include <mksync/base/base_library.h>

#include <nekoproto/proto/json_serializer.hpp>
#include <nekoproto/proto/serializer_base.hpp>
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>

using NekoProto::detail::OutputSerializer;

namespace NekoProto
{
    template <typename Allocator>
    class JsonOutput : public OutputSerializer<JsonOutput<Allocator>> {
    public:
        using BufferType = rapidjson::Value;

        enum Type
        {
            eValue,
            eArray,
            eObject,
        };

    public:
        JsonOutput(BufferType &out, Allocator &allocator) NEKO_NOEXCEPT
            : OutputSerializer<JsonOutput>(this),
              _buffer(out),
              _allocator(allocator)
        {
            _typeStack.push_back(eValue);
            _type = _typeStack.back();
            _valueStack.push_back(&_buffer);
            _currentValue = _valueStack.back();
        }
        JsonOutput(const JsonOutput &other) NEKO_NOEXCEPT : OutputSerializer<JsonOutput>(this),
                                                            _buffer(other._buffer),
                                                            _allocator(other._allocator),
                                                            _typeStack(other._typeStack),
                                                            _valueStack(other._valueStack),
                                                            _type(other._type),
                                                            _currentValue(other._currentValue)
        {
        }
        JsonOutput(JsonOutput &&other) NEKO_NOEXCEPT : OutputSerializer<JsonOutput>(this),
                                                       _buffer(other._buffer),
                                                       _allocator(other._allocator),
                                                       _typeStack(other._typeStack),
                                                       _valueStack(other._valueStack),
                                                       _type(other._type),
                                                       _currentValue(other._currentValue)
        {
        }
        template <typename T>
        inline bool saveValue([[maybe_unused]] const SizeTag<T> &size) NEKO_NOEXCEPT
        {
            return true;
        }
        template <typename T,
                  traits::enable_if_t<std::is_signed<T>::value, sizeof(T) <= sizeof(int64_t),
                                      !std::is_enum<T>::value> = traits::default_value_for_enable>
        inline bool saveValue(const T value) NEKO_NOEXCEPT
        {
            switch (_type) {
            case eValue:
                _currentValue->SetInt64(value);
                break;
            case eArray: {
                rapidjson::Value jvalue;
                jvalue.SetInt64(value);
                _currentValue->PushBack(jvalue, _allocator);
            } break;
            case eObject: {
                rapidjson::Value jvalue;
                jvalue.SetInt64(value);
                rapidjson::Value name;
                name.SetString(_name.data(), (int)_name.size(), _allocator);
                _currentValue->AddMember(name, jvalue, _allocator);
            }
            }
            return true;
        }
        template <typename T,
                  traits::enable_if_t<std::is_unsigned<T>::value, sizeof(T) <= sizeof(int64_t),
                                      !std::is_enum<T>::value> = traits::default_value_for_enable>
        inline bool saveValue(const T value) NEKO_NOEXCEPT
        {
            switch (_type) {
            case eValue:
                _currentValue->SetUint64(value);
                break;
            case eArray: {
                rapidjson::Value jvalue;
                jvalue.SetUint64(value);
                _currentValue->PushBack(jvalue, _allocator);
            } break;
            case eObject: {
                rapidjson::Value jvalue;
                jvalue.SetUint64(value);
                rapidjson::Value name;
                name.SetString(_name.data(), _name.size(), _allocator);
            }
            }
            return true;
        }
        inline bool saveValue(const double value) NEKO_NOEXCEPT
        {
            switch (_type) {
            case eValue:
                _currentValue->SetDouble(value);
                break;
            case eArray: {
                rapidjson::Value jvalue;
                jvalue.SetDouble(value);
                _currentValue->PushBack(jvalue, _allocator);
            } break;
            case eObject: {
                rapidjson::Value jvalue;
                jvalue.SetDouble(value);
                rapidjson::Value name;
                name.SetString(_name.data(), _name.size(), _allocator);
                _currentValue->AddMember(name, jvalue, _allocator);
            }
            }
            return true;
        }
        inline bool saveValue(const float value) NEKO_NOEXCEPT { return saveValue((double)value); }
        inline bool saveValue(const bool value) NEKO_NOEXCEPT
        {
            switch (_type) {
            case eValue:
                _currentValue->SetBool(value);
                break;
            case eArray: {
                rapidjson::Value jvalue;
                jvalue.SetBool(value);
                _currentValue->PushBack(jvalue, _allocator);
            } break;
            case eObject: {
                rapidjson::Value jvalue;
                jvalue.SetBool(value);
                rapidjson::Value name;
                name.SetString(_name.data(), _name.size(), _allocator);
                _currentValue->AddMember(name, jvalue, _allocator);
            }
            }
            return true;
        }
        inline bool saveValue(const std::string_view value) NEKO_NOEXCEPT
        {
            switch (_type) {
            case eValue:
                _currentValue->SetString(value.data(), (int)value.length(), _allocator);
                break;
            case eArray: {
                rapidjson::Value jvalue;
                jvalue.SetString(value.data(), (int)value.length(), _allocator);
                _currentValue->PushBack(jvalue, _allocator);
            } break;
            case eObject: {
                rapidjson::Value jvalue;
                jvalue.SetString(value.data(), (int)value.length(), _allocator);
                rapidjson::Value name;
                name.SetString(_name.data(), (int)_name.size(), _allocator);
                _currentValue->AddMember(name, jvalue, _allocator);
            }
            }
            return true;
        }
        template <typename CharT, typename Traits, typename Alloc>
        bool saveValue(const std::basic_string<CharT, Traits, Alloc> &value) NEKO_NOEXCEPT
        {
            return saveValue(std::string_view(value));
        }
        inline bool saveValue(const char *value) NEKO_NOEXCEPT
        {
            return saveValue(std::string_view(value));
        }
        template <typename T>
        inline bool saveValue(const NameValuePair<T> &value) NEKO_NOEXCEPT
        {
            if constexpr (traits::is_optional<T>::value) {
                if (value.value.has_value()) {
                    _name = std::string_view(value.name, value.nameLen);
                    return (*this)(value.value.value());
                }
                return true;
            }
            else {
                _name = std::string_view{value.name, value.nameLen};
                return (*this)(value.value);
            }
        }
        bool startArray(const std::size_t) NEKO_NOEXCEPT
        {
            if (_type == eObject) {
                _currentValue = &_valueStack.back()->AddMember(
                    rapidjson::Value(_name.data(), _name.size()),
                    rapidjson::Value(rapidjson::kArrayType), _allocator);
                _valueStack.push_back(_currentValue);
            }
            else if (_type == eArray) {
                _currentValue = &_valueStack.back()->PushBack(
                    rapidjson::Value(rapidjson::kArrayType), _allocator);
                _valueStack.push_back(_currentValue);
            }
            _typeStack.push_back(eArray);
            _type = eArray;
            return true;
        }
        bool endArray() NEKO_NOEXCEPT
        {
            _typeStack.pop_back();
            _type = _typeStack.back();
            if (_type == eObject) {
                _valueStack.pop_back();
                _currentValue = _valueStack.back();
            }
            else if (_type == eArray) {
                _valueStack.pop_back();
                _currentValue = _valueStack.back();
            }
            return true;
        }
        bool startObject(const std::size_t /*unused*/) NEKO_NOEXCEPT
        {
            if (_type == eObject) {
                _currentValue = &_valueStack.back()->AddMember(
                    rapidjson::Value(_name.data(), (int)_name.size()),
                    rapidjson::Value(rapidjson::kObjectType), _allocator);
                _valueStack.push_back(_currentValue);
            }
            else if (_type == eArray) {
                _currentValue = &_valueStack.back()->PushBack(
                    rapidjson::Value(rapidjson::kObjectType), _allocator);
                _valueStack.push_back(_currentValue);
            }
            _typeStack.push_back(eObject);
            _type = eObject;
            return true;
        }
        bool endObject() NEKO_NOEXCEPT
        {
            _typeStack.pop_back();
            _type = _typeStack.back();
            if (_type == eObject) {
                _valueStack.pop_back();
                _currentValue = _valueStack.back();
            }
            else if (_type == eArray) {
                _valueStack.pop_back();
                _currentValue = _valueStack.back();
            }
            return true;
        }
        bool end() NEKO_NOEXCEPT { return true; }

    private:
        JsonOutput &operator=(const JsonOutput &) = delete;
        JsonOutput &operator=(JsonOutput &&)      = delete;

    private:
        BufferType                     &_buffer;
        Allocator                      &_allocator;
        std::vector<rapidjson::Value *> _valueStack;
        rapidjson::Value               *_currentValue;
        std::vector<Type>               _typeStack;
        Type                            _type = eValue;
        std::string_view                _name;
    };

    class JsonInput : public detail::InputSerializer<JsonInput> {
    public:
        explicit JsonInput(const rapidjson::Value &value) NEKO_NOEXCEPT
            : detail::InputSerializer<JsonInput>(this),
              _value(value),
              _itemStack()
        {
        }
        ~JsonInput() NEKO_NOEXCEPT {}
        NEKO_STRING_VIEW name() const NEKO_NOEXCEPT
        {
            NEKO_ASSERT(_currentItem != nullptr, "JsonSerializer", "Current Item is nullptr");
            if ((*_currentItem).eof()) {
                return {};
            }
            return (*_currentItem).name();
        }

        template <typename T,
                  traits::enable_if_t<std::is_signed<T>::value, sizeof(T) < sizeof(int64_t),
                                      !std::is_enum<T>::value> = traits::default_value_for_enable>
        bool loadValue(T &value) NEKO_NOEXCEPT
        {
            NEKO_ASSERT(_currentItem != nullptr, "JsonSerializer", "Current Item is nullptr");
            if (!(*_currentItem).value().IsInt()) {
                return false;
            }
            value = static_cast<T>((*_currentItem).value().GetInt());
            ++(*_currentItem);
            return true;
        }

        template <typename T,
                  traits::enable_if_t<std::is_unsigned<T>::value, sizeof(T) < sizeof(uint64_t),
                                      !std::is_enum<T>::value> = traits::default_value_for_enable>
        bool loadValue(T &value) NEKO_NOEXCEPT
        {
            NEKO_ASSERT(_currentItem != nullptr, "JsonSerializer", "Current Item is nullptr");
            if (!(*_currentItem).value().IsUint()) {
                return false;
            }
            value = static_cast<T>((*_currentItem).value().GetUint());
            ++(*_currentItem);
            return true;
        }

        template <typename CharT, typename Traits, typename Alloc>
        bool loadValue(std::basic_string<CharT, Traits, Alloc> &value) NEKO_NOEXCEPT
        {
            NEKO_ASSERT(_currentItem != nullptr, "JsonSerializer", "Current Item is nullptr");
            if (!(*_currentItem).value().IsString()) {
                return false;
            }
            const auto &cvalue = (*_currentItem).value();
            value              = std::basic_string<CharT, Traits, Alloc>{cvalue.GetString(),
                                                                         cvalue.GetStringLength()};
            ++(*_currentItem);
            return true;
        }

        bool loadValue(int64_t &value) NEKO_NOEXCEPT
        {
            NEKO_ASSERT(_currentItem != nullptr, "JsonSerializer", "Current Item is nullptr");
            if (!(*_currentItem).value().IsInt64()) {
                return false;
            }
            value = (*_currentItem).value().GetInt64();
            ++(*_currentItem);
            return true;
        }

        bool loadValue(uint64_t &value) NEKO_NOEXCEPT
        {
            NEKO_ASSERT(_currentItem != nullptr, "JsonSerializer", "Current Item is nullptr");
            if (!(*_currentItem).value().IsUint64()) {
                return false;
            }
            value = (*_currentItem).value().GetUint64();
            ++(*_currentItem);
            return true;
        }

        bool loadValue(float &value) NEKO_NOEXCEPT
        {
            NEKO_ASSERT(_currentItem != nullptr, "JsonSerializer", "Current Item is nullptr");
            if (!(*_currentItem).value().IsNumber()) {
                return false;
            }
            value = (float)(*_currentItem).value().GetDouble();
            ++(*_currentItem);
            return true;
        }

        bool loadValue(double &value) NEKO_NOEXCEPT
        {
            NEKO_ASSERT(_currentItem != nullptr, "JsonSerializer", "Current Item is nullptr");
            if (!(*_currentItem).value().IsNumber()) {
                return false;
            }
            value = (*_currentItem).value().GetDouble();
            ++(*_currentItem);
            return true;
        }

        bool loadValue(bool &value) NEKO_NOEXCEPT
        {
            NEKO_ASSERT(_currentItem != nullptr, "JsonSerializer", "Current Item is nullptr");
            if (!(*_currentItem).value().IsBool()) {
                return false;
            }
            value = (*_currentItem).value().GetBool();
            ++(*_currentItem);
            return true;
        }

        bool loadValue(std::nullptr_t) NEKO_NOEXCEPT
        {
            NEKO_ASSERT(_currentItem != nullptr, "JsonSerializer", "Current Item is nullptr");
            if (!(*_currentItem).value().IsNull()) {
                return false;
            }
            ++(*_currentItem);
            return true;
        }

        template <typename T>
        bool loadValue(const SizeTag<T> &value) NEKO_NOEXCEPT
        {
            NEKO_ASSERT(_currentItem != nullptr, "JsonSerializer", "Current Item is nullptr");
            value.size = (*_currentItem).size();
            return true;
        }

#if NEKO_CPP_PLUS >= 17
        template <typename T>
        bool loadValue(const NameValuePair<T> &value) NEKO_NOEXCEPT
        {
            NEKO_ASSERT(_currentItem != nullptr, "JsonSerializer", "Current Item is nullptr");
            const auto &cvalue = (*_currentItem).moveToMember({value.name, value.nameLen});
            bool        ret    = true;
            if constexpr (traits::is_optional<T>::value) {
                if (nullptr == cvalue || cvalue->IsNull()) {
                    value.value.reset();
    #if defined(NEKO_VERBOSE_LOGS)
                    NEKO_LOG_INFO("JsonSerializer", "optional field {} is not find.",
                                  std::string(value.name, value.nameLen));
    #endif
                }
                else {
    #if defined(NEKO_JSON_MAKE_STR_NONE_TO_NULL)
                    // Why would anyone write "None" in json?
                    // I've seen it, and it's a disaster.
                    if (cvalue->IsString() && std::strcmp(cvalue->GetString(), "None") == 0) {
                        value.value.reset();
        #if defined(NEKO_VERBOSE_LOGS)
                        NEKO_LOG_WARN("JsonSerializer", "optional field {} is \"None\".",
                                      std::string(value.name, value.nameLen));
        #endif
                        return true;
                    }
    #endif
                    typename traits::is_optional<T>::value_type result;
                    ret = (*this)(result);
                    value.value.emplace(std::move(result));
    #if defined(NEKO_VERBOSE_LOGS)
                    if (ret) {
                        NEKO_LOG_INFO("JsonSerializer", "optional field {} get value success.",
                                      std::string(value.name, value.nameLen));
                    }
                    else {
                        NEKO_LOG_ERROR("JsonSerializer", "optional field {} get value fail.",
                                       std::string(value.name, value.nameLen));
                    }
    #endif
                }
            }
            else {
                if (nullptr == cvalue) {
    #if defined(NEKO_VERBOSE_LOGS)
                    NEKO_LOG_ERROR("JsonSerializer", "field {} is not find.",
                                   std::string(value.name, value.nameLen));
    #endif
                    return false;
                }
                ret = (*this)(value.value);
    #if defined(NEKO_VERBOSE_LOGS)
                if (ret) {
                    NEKO_LOG_INFO("JsonSerializer", "field {} get value success.",
                                  std::string(value.name, value.nameLen));
                }
                else {
                    NEKO_LOG_ERROR("JsonSerializer", "field {} get value fail.",
                                   std::string(value.name, value.nameLen));
                }
    #endif
            }
            return ret;
        }
#else
        template <typename T>
        bool loadValue(const NameValuePair<T> &value) NEKO_NOEXCEPT
        {
            NEKO_ASSERT(_currentItem != nullptr, "JsonSerializer", "Current Item is nullptr");
            const auto &v = (*_currentItem).move_to_member({value.name, value.nameLen});
            if (nullptr == v) {
    #if defined(NEKO_VERBOSE_LOGS)
                NEKO_LOG_ERROR("JsonSerializer", "field {} is not find.",
                               std::string(value.name, value.nameLen));
    #endif
                return false;
            }
    #if defined(NEKO_VERBOSE_LOGS)
            auto ret = operator()(value.value);
            if (ret) {
                NEKO_LOG_INFO("JsonSerializer", "field {} get value success.",
                              std::string(value.name, value.nameLen));
            }
            else {
                NEKO_LOG_ERROR("JsonSerializer", "field {} get value fail.",
                               std::string(value.name, value.nameLen));
            }
            return ret;
    #else
            return operator()(value.value);
    #endif
        }
#endif
        bool startNode() NEKO_NOEXCEPT
        {
            if (!_itemStack.empty()) {
                if ((*_currentItem).value().IsArray()) {
                    _itemStack.emplace_back((*_currentItem).value().Begin(),
                                            (*_currentItem).value().End());
                }
                else if ((*_currentItem).value().IsObject()) {
                    _itemStack.emplace_back((*_currentItem).value().MemberBegin(),
                                            (*_currentItem).value().MemberEnd());
                }
                else {
                    return false;
                }
            }
            else {
                if (_value.IsArray()) {
                    _itemStack.emplace_back(_value.Begin(), _value.End());
                }
                else if (_value.IsObject()) {
                    _itemStack.emplace_back(_value.MemberBegin(), _value.MemberEnd());
                }
                else {
                    return false;
                }
            }
            _currentItem = &_itemStack.back();
            return true;
        }

        bool finishNode() NEKO_NOEXCEPT
        {
            if (_itemStack.size() >= 2) {
                _itemStack.pop_back();
                _currentItem = &_itemStack.back();
                ++(*_currentItem);
            }
            else if (_itemStack.size() == 1) {
                _itemStack.pop_back();
                _currentItem = nullptr;
            }
            else {
                return false;
            }
            return true;
        }

    private:
        JsonInput &operator=(const JsonInput &) = delete;
        JsonInput &operator=(JsonInput &&)      = delete;

    private:
        const rapidjson::Value                &_value;
        std::vector<detail::ConstJsonIterator> _itemStack;
        detail::ConstJsonIterator             *_currentItem = nullptr;
    };

    template <typename T>
    inline bool prologue(JsonInput & /*unused*/, const NameValuePair<T> & /*unused*/) NEKO_NOEXCEPT
    {
        return true;
    }
    template <typename T>
    inline bool epilogue(JsonInput & /*unused*/, const NameValuePair<T> & /*unused*/) NEKO_NOEXCEPT
    {
        return true;
    }

    template <typename T, typename Allocator>
    inline bool prologue(JsonOutput<Allocator> & /*unused*/,
                         const NameValuePair<T> & /*unused*/) NEKO_NOEXCEPT
    {
        return true;
    }
    template <typename T, typename Allocator>
    inline bool epilogue(JsonOutput<Allocator> & /*unused*/,
                         const NameValuePair<T> & /*unused*/) NEKO_NOEXCEPT
    {
        return true;
    }

    // #########################################################
    //  size tag
    template <typename T>
    inline bool prologue(JsonInput & /*unused*/, const SizeTag<T> & /*unused*/) NEKO_NOEXCEPT
    {
        return true;
    }
    template <typename T>
    inline bool epilogue(JsonInput & /*unused*/, const SizeTag<T> & /*unused*/) NEKO_NOEXCEPT
    {
        return true;
    }

    template <typename T, typename Allocator>
    inline bool prologue(JsonOutput<Allocator> & /*unused*/,
                         const SizeTag<T> & /*unused*/) NEKO_NOEXCEPT
    {
        return true;
    }
    template <typename T, typename Allocator>
    inline bool epilogue(JsonOutput<Allocator> & /*unused*/,
                         const SizeTag<T> & /*unused*/) NEKO_NOEXCEPT
    {
        return true;
    }

    // #########################################################
    // class apart from name value pair, size tag, std::string, NEKO_STRING_VIEW
    template <class T,
              traits::enable_if_t<std::is_class<T>::value, !is_minimal_serializable<T>::value> =
                  traits::default_value_for_enable>
    inline bool prologue(JsonInput &sa, const T & /*unused*/) NEKO_NOEXCEPT
    {
        return sa.startNode();
    }
    template <typename T,
              traits::enable_if_t<std::is_class<T>::value, !is_minimal_serializable<T>::value> =
                  traits::default_value_for_enable>
    inline bool epilogue(JsonInput &sa, const T & /*unused*/) NEKO_NOEXCEPT
    {
        return sa.finishNode();
    }

    template <
        class T, typename Allocator,
        traits::enable_if_t<std::is_class<T>::value, !std::is_same<std::string, T>::value,
                            !is_minimal_serializable<T>::valueT,
                            !traits::has_method_const_serialize<T, JsonOutput<Allocator>>::value> =
            traits::default_value_for_enable>
    inline bool prologue(JsonOutput<Allocator> & /*unused*/, const T & /*unused*/) NEKO_NOEXCEPT
    {
        return true;
    }

    template <
        typename T, typename Allocator,
        traits::enable_if_t<std::is_class<T>::value, !is_minimal_serializable<T>::valueT,
                            !traits::has_method_const_serialize<T, JsonOutput<Allocator>>::value> =
            traits::default_value_for_enable>
    inline bool epilogue(JsonOutput<Allocator> & /*unused*/, const T & /*unused*/) NEKO_NOEXCEPT
    {
        return true;
    }

    template <
        typename T, typename Allocator,
        traits::enable_if_t<traits::has_method_const_serialize<T, JsonOutput<Allocator>>::value ||
                            traits::has_method_serialize<T, JsonOutput<Allocator>>::value> =
            traits::default_value_for_enable>
    inline bool prologue(JsonOutput<Allocator> &sa, const T & /*unused*/) NEKO_NOEXCEPT
    {
        return sa.startObject((std::size_t)-1);
    }
    template <
        typename T, typename Allocator,
        traits::enable_if_t<traits::has_method_const_serialize<T, JsonOutput<Allocator>>::value ||
                            traits::has_method_serialize<T, JsonOutput<Allocator>>::value> =
            traits::default_value_for_enable>
    inline bool epilogue(JsonOutput<Allocator> &sa, const T & /*unused*/) NEKO_NOEXCEPT
    {
        return sa.endObject();
    }

    // #########################################################
    // # arithmetic types
    template <typename T,
              traits::enable_if_t<std::is_arithmetic<T>::value> = traits::default_value_for_enable>
    inline bool prologue(JsonInput & /*unused*/, const T & /*unused*/) NEKO_NOEXCEPT
    {
        return true;
    }
    template <typename T,
              traits::enable_if_t<std::is_arithmetic<T>::value> = traits::default_value_for_enable>
    inline bool epilogue(JsonInput & /*unused*/, const T & /*unused*/) NEKO_NOEXCEPT
    {
        return true;
    }

    template <typename T, typename Allocator,
              traits::enable_if_t<std::is_arithmetic<T>::value> = traits::default_value_for_enable>
    inline bool prologue(JsonOutput<Allocator> & /*unused*/, const T & /*unused*/) NEKO_NOEXCEPT
    {
        return true;
    }
    template <typename T, typename Allocator,
              traits::enable_if_t<std::is_arithmetic<T>::value> = traits::default_value_for_enable>
    inline bool epilogue(JsonOutput<Allocator> & /*unused*/, const T & /*unused*/) NEKO_NOEXCEPT
    {
        return true;
    }

    // #####################################################
    // # std::string
    template <typename CharT, typename Traits, typename Alloc>
    inline bool prologue(JsonInput & /*unused*/,
                         const std::basic_string<CharT, Traits, Alloc> & /*unused*/) NEKO_NOEXCEPT
    {
        return true;
    }
    template <typename CharT, typename Traits, typename Alloc>
    inline bool epilogue(JsonInput & /*unused*/,
                         const std::basic_string<CharT, Traits, Alloc> & /*unused*/) NEKO_NOEXCEPT
    {
        return true;
    }

    template <typename CharT, typename Traits, typename Alloc, typename Allocator>
    inline bool prologue(JsonOutput<Allocator> & /*unused*/,
                         const std::basic_string<CharT, Traits, Alloc> & /*unused*/) NEKO_NOEXCEPT
    {
        return true;
    }
    template <typename CharT, typename Traits, typename Alloc, typename Allocator>
    inline bool epilogue(JsonOutput<Allocator> & /*unused*/,
                         const std::basic_string<CharT, Traits, Alloc> & /*unused*/) NEKO_NOEXCEPT
    {
        return true;
    }

#if NEKO_CPP_PLUS >= 17
    template <typename CharT, typename Traits, typename Allocator>
    inline bool prologue(JsonOutput<Allocator> & /*unused*/,
                         const std::basic_string_view<CharT, Traits> & /*unused*/) NEKO_NOEXCEPT
    {
        return true;
    }
    template <typename CharT, typename Traits, typename Allocator>
    inline bool epilogue(JsonOutput<Allocator> & /*unused*/,
                         const std::basic_string_view<CharT, Traits> & /*unused*/) NEKO_NOEXCEPT
    {
        return true;
    }
#endif

    // #####################################################
    // # std::nullptr_t
    inline bool prologue(JsonInput & /*unused*/, const std::nullptr_t) NEKO_NOEXCEPT
    {
        return true;
    }
    inline bool epilogue(JsonInput & /*unused*/, const std::nullptr_t) NEKO_NOEXCEPT
    {
        return true;
    }

    template <typename Allocator>
    inline bool prologue(JsonOutput<Allocator> & /*unused*/, const std::nullptr_t) NEKO_NOEXCEPT
    {
        return true;
    }
    template <typename Allocator>
    inline bool epilogue(JsonOutput<Allocator> & /*unused*/, const std::nullptr_t) NEKO_NOEXCEPT
    {
        return true;
    }
    // #####################################################
    // # minimal serializable
    template <typename T, traits::enable_if_t<is_minimal_serializable<T>::value> =
                              traits::default_value_for_enable>
    inline bool prologue(JsonInput & /*unused*/, const T & /*unused*/) NEKO_NOEXCEPT
    {
        return true;
    }
    template <typename T, traits::enable_if_t<is_minimal_serializable<T>::value> =
                              traits::default_value_for_enable>
    inline bool epilogue(JsonInput & /*unused*/, const T & /*unused*/) NEKO_NOEXCEPT
    {
        return true;
    }

    template <
        typename T, typename Allocator,
        traits::enable_if_t<is_minimal_serializable<T>::value> = traits::default_value_for_enable>
    inline bool prologue(JsonOutput<Allocator> & /*unused*/, const T & /*unused*/) NEKO_NOEXCEPT
    {
        return true;
    }
    template <
        typename T, typename Allocator,
        traits::enable_if_t<is_minimal_serializable<T>::value> = traits::default_value_for_enable>
    inline bool epilogue(JsonOutput<Allocator> & /*unused*/, const T & /*unused*/) NEKO_NOEXCEPT
    {
        return true;
    }
} // namespace NekoProto

MKS_BEGIN

namespace detail
{
    template <typename T, typename Allocator>
        requires requires(NekoProto::JsonOutput<Allocator> out, T val) {
            { val.serialize(out) } -> std::same_as<bool>;
        }
    rapidjson::Value to_json_value(const T &value, Allocator &allocator)
    {
        rapidjson::Value jvalue(rapidjson::kObjectType);
        if (NekoProto::JsonOutput<Allocator> out(jvalue, allocator); out(value)) {
            return jvalue;
        }
        return rapidjson::Value(rapidjson::kNullType);
    }

    template <typename T>
        requires requires(NekoProto::JsonInput in, T val) {
            { val.serialize(in) } -> std::same_as<bool>;
        }
    bool from_json_value(const rapidjson::Value &value, T &result)
    {
        if (!value.IsObject()) {
            return false;
        }
        NekoProto::JsonInput in(value);
        return in(result);
    }
} // namespace detail

MKS_END