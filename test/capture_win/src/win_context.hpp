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

#include <ilias/platform.hpp>
#include <ilias/platform/delegate.hpp>

/**
 * @brief The main context
 * windows的事件循环与协程兼容上下文。
 */
class WinContext final
    : public ILIAS_NAMESPACE::DelegateContext<ILIAS_NAMESPACE::PlatformContext> { //< Delegate the
                                                                                  // io to the iocp
public:
    WinContext();
    ~WinContext();

    auto post(void (*fn)(void *), void *arg) -> void override;
    auto run(ILIAS_NAMESPACE::CancellationToken &token) -> void override;

private:
    auto wndproc(UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT;

    HWND mHwnd = nullptr; //< The window handle for dispatching the messages
};