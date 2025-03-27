/**
 * @file settings.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief save settings and load settings
 * @version 0.1
 * @date 2025-03-12
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include <filesystem>
#include <nekoproto/proto/proto_base.hpp>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <spdlog/spdlog.h>

#include "mksync/base/base_library.h"
#include "mksync/base/detail/help_type_traits.hpp"

namespace mks::base
{
    /**
     * @brief save settings and load settings
     *
     */
    class MKS_BASE_API Settings {
    public:
        Settings();
        Settings(std::string_view file);
        ~Settings();

        template <typename T>
            requires requires(T va) {
                detail::to_json_value(va, std::declval<RAPIDJSON_DEFAULT_ALLOCATOR &>());
            }
        void set(std::string_view key, const T &value)
        {
            rapidjson::Value name(key.data(), (int)key.size(), _document.GetAllocator());
            if (_document.IsObject()) {
                auto item = _document.FindMember(name);
                if (item != _document.MemberEnd()) {
                    item->value = detail::to_json_value(value, _document.GetAllocator());
                    return;
                }
            }
            else {
                _document.SetObject();
            }
            _document.AddMember(name, detail::to_json_value(value, _document.GetAllocator()),
                                _document.GetAllocator());
        }

        template <typename T>
            requires requires(T va) { detail::from_json_value(rapidjson::Value(), va); }
        T get(std::string_view key, const T &defaultValue) const
        {
            rapidjson::Value name(key.data(), (rapidjson::SizeType)key.size());
            auto             item = _document.FindMember(name);
            if (item == _document.MemberEnd()) {
                SPDLOG_ERROR("config option {} not found", key);
                return defaultValue;
            }
            T result;
            if (detail::from_json_value(item->value, result)) {
                return result;
            }
            SPDLOG_ERROR("config option {} type not match", key);
            return defaultValue;
        }

        bool save() const;
        bool save(std::string_view file);
        bool load();
        bool load(std::string_view file);
        auto get_file() const { return _file; }

    private:
        std::filesystem::path _file;
        rapidjson::Document   _document;
    };
} // namespace mks::base