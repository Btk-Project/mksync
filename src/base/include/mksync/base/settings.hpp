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

    /**
     * @brief 设置指定配置项的值
     * @note 如果配置项存在就修改，不存在就创建。
     * @tparam T 值类型
     * @param key 配置名
     * @param value 值
     */
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

    /**
     * @brief 获取指定配置的值
     * @note 如果指定配置名不存在或类型错误则返回默认值
     * @tparam T 类型
     * @param key 配置名
     * @param defaultValue 默认值
     * @return requires
     */
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

    /**
     * @brief 保存所有配置到本地文件
     *
     * @return true
     * @return false
     */
    bool save() const;
    /**
     * @brief 保存所有配置到指定文件
     * @note 配置采用json格式保存
     * @param file 配置文件
     * @return true
     * @return false
     */
    bool save(std::string_view file);
    /**
     * @brief 从本地文件加载配置
     *
     * @return true
     * @return false
     */
    bool load();
    /**
     * @brief 从指定文件加载配置
     *
     * @param file 配置文件
     * @return true
     * @return false
     */
    bool load(std::string_view file);

    /**
     * @brief 获取当前配置文件路径
     *
     * @return std::filesystem::path
     */
    auto get_file() const { return _file; }

    private:
        std::filesystem::path _file;
        rapidjson::Document   _document;
    };
} // namespace mks::base