#include "win_context.hpp"

template <typename T>
using DelegateContext = ILIAS_NAMESPACE::DelegateContext<T>;
template <typename T>
using Task = ILIAS_NAMESPACE::Task<T>;
template <typename T>
using IoTask = ILIAS_NAMESPACE::IoTask<T>;

WinContext::WinContext()
{
    ::WNDCLASSEXW wc{
        .cbSize      = sizeof(::WNDCLASSEXW),
        .lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
            auto self = reinterpret_cast<WinContext *>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (self) {
                return self->wndproc(msg, wParam, lParam);
            }
            return ::DefWindowProcW(hwnd, msg, wParam, lParam);
        },
        .hInstance     = ::GetModuleHandleW(nullptr),
        .lpszClassName = L"WinContext"};
    if (!::RegisterClassExW(&wc)) {
        throw std::runtime_error("Failed to register window class");
    }
    mHwnd = ::CreateWindowExW(0, wc.lpszClassName, L"WinContext", WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, CW_USEDEFAULT, 10, 10,
                              HWND_MESSAGE, //< Only for message
                              nullptr, ::GetModuleHandleW(nullptr), nullptr);
    if (!mHwnd) {
        throw std::runtime_error("Failed to create window");
    }
    ::SetWindowLongPtrW(mHwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
}

WinContext::~WinContext()
{
    ::DestroyWindow(mHwnd);
}

auto WinContext::post(void (*fn)(void *), void *arg) -> void
{
    // 推送事件，唤起事件循环，并将回调函数传递过去
    ::PostMessageW(mHwnd, WM_USER, reinterpret_cast<WPARAM>(fn), reinterpret_cast<LPARAM>(arg));
}

auto WinContext::run(ILIAS_NAMESPACE::CancellationToken &token) -> void
{
    BOOL stop = 0;
    auto reg  = token.register_([&]() { // 注册一个用来退出该事件循环的回调。
        stop = true;
        post(nullptr, nullptr); //< Wakeup the message loop
    });
    while (stop == 0) { // 主事件循环
        MSG msg;
        if (::GetMessageW(&msg, nullptr, 0, 0) != 0) {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
        }
    }
}

auto WinContext::wndproc(UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT
{
    switch (msg) {
    case WM_USER: {
        auto fn = reinterpret_cast<void (*)(void *)>(wParam);
        if (fn != nullptr) { // 执行用户post的回调
            fn(reinterpret_cast<void *>(lParam));
        }
        break;
    }
    }
    return ::DefWindowProcW(mHwnd, msg, wParam, lParam);
}
