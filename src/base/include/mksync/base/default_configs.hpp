/**
 * @file default_configs.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-03-18
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include <string>

#include "mksync/base/base_library.h"
#include "mksync/proto/config_proto.hpp"

// base config table, configuration column
// config name | type | default value | description
#define MKS_BASE_CONFIG_TABLE                                                                      \
    MKS_BASE_CONFIG(screen_name, std::string_view, "unknow", "This screen name.")                  \
    MKS_BASE_CONFIG(max_log_records, int, 1000, "Maximum number of log records.")                  \
    MKS_BASE_CONFIG(log_level, std::string_view, "warn", "Log level for print to console.")        \
    MKS_BASE_CONFIG(log_file, std::string_view, "", "Save log in this file if it set.")            \
    MKS_BASE_CONFIG(module_list, std::vector<std::string_view>, {}, "Module list to load.")        \
    MKS_BASE_CONFIG(screen_settings, std::vector<VirtualScreenConfig>, {}, "Screen's settings.")   \
    MKS_BASE_CONFIG(server_ipaddress, std::string_view, "0.0.0.0:8577", "sever ip")                \
    MKS_BASE_CONFIG(remote_controller, std::string_view, "tcp:127.0.0.1:8578",                     \
                    "Remote controller type.")

namespace mks::base
{
#define MKS_BASE_CONFIG(config_name, type, default_value, description)                             \
    inline static constexpr const type  config_name##_default_value      = default_value;          \
    inline static constexpr const char *config_name##_config_description = description;            \
    inline static constexpr const char *config_name##_config_name        = #config_name;
    MKS_BASE_CONFIG_TABLE
#undef MKS_BASE_CONFIG

} // namespace mks::base
#undef MKS_BASE_CONFIG_TABLE