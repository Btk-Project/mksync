/**
 * @file win_context.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-02-22
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once
#ifdef _WIN32
    #include <ilias/platform.hpp>
    #include <ilias/platform/delegate.hpp>
    #include "mksync/base/base_library.h"

namespace mks::base
{
    /**
     * @brief The main context
     * windows的事件循环与协程兼容上下文。
     */
    class MKS_BASE_API WinContext final
        : public ::ilias::DelegateContext<::ilias::PlatformContext> { //< Delegate the
                                                                      // io to the iocp
    public:
        WinContext();
        ~WinContext();

        auto post(void (*fn)(void *), void *arg) -> void override;
        auto run(::ilias::CancellationToken &token) -> void override;

    private:
        auto _wndproc(UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT;

        HWND _hwnd = nullptr; //< The window handle for dispatching the messages
    };
} // namespace mks::base
#endif