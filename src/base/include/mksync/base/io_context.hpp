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
    #include "mksync/base/platform/win_context.hpp"
namespace mks::base
{
    using IoContext = WinContext;
}
#elif defined(__linux__)
    #include <ilias/platform.hpp>
namespace mks::base
{
    using IoContext = ::ilias::PlatformContext;
}
#endif