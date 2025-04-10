/**
 * @file io_context.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-02-23
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#if defined(_WIN32)
    #include "mksync/core/platform/win_context.hpp"
MKS_BEGIN
using IoContext = WinContext;
MKS_END
#elif defined(__linux__)
    #include <ilias/platform.hpp>
MKS_BEGIN
using IoContext = ::ilias::PlatformContext;
MKS_END
#endif