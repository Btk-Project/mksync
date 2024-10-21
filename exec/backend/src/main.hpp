#pragma once

#include "mksync/base/osdep.h"

#include <ilias/task.hpp>

// clang-format off
#define MKS_BACKED_LOG_TRACE(...)       SPDLOG_TRACE(...)
#define MKS_BACKED_LOG_DEBUG(...)       SPDLOG_DEBUG(...)
#define MKS_BACKED_LOG_INFO(...)        SPDLOG_INFO(...)
#define MKS_BACKED_LOG_WARN(...)        SPDLOG_WARN(...)
#define MKS_BACKED_LOG_ERROR(...)       SPDLOG_ERROR(...)
#define MKS_BACKED_LOG_CRITICAL(...)    SPDLOG_CRITICAL(...)
// clang-format on

ilias::Task<void> main_loop();    // contains the following three event loops
ilias::Task<void> console_loop(); // front-back end Command loop
ilias::Task<void> network_loop(); // client-server network event loop
ilias::Task<void> daemon_loop();  // command loops with the daemon