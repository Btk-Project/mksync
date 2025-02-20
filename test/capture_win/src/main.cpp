#include <ilias/ilias.hpp>
#include <ilias/platform.hpp>
#include <ilias/platform/delegate.hpp>
#include <ilias/sync/event.hpp>
#include <ilias/net/tcp.hpp>
#include <ilias/fs/console.hpp>
#include <nekoproto/communication/communication_base.hpp>
#include <iostream>
#include <Windows.h>
#include <spdlog/spdlog.h>

#include "mksync/proto/proto.hpp"

template <typename T>
using DelegateContext = ILIAS_NAMESPACE::DelegateContext<T>;
template <typename T>
using Task = ILIAS_NAMESPACE::Task<T>;
template <typename T>
using IoTask      = ILIAS_NAMESPACE::IoTask<T>;
using IPEndpoint  = ILIAS_NAMESPACE::IPEndpoint;
using TcpClient   = ILIAS_NAMESPACE::TcpClient;
using TcpListener = ILIAS_NAMESPACE::TcpListener;
/**
 * @brief The main context
 * windows的事件循环与协程兼容上下文。
 */
class WinContext final
    : public DelegateContext<ILIAS_NAMESPACE::PlatformContext> { //< Delegate the
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

/**
 * @brief ringbuffer
 * 一个循环队列，用于存储事件，为了避免没有消耗的鼠标键盘事件大量堆积，因此该队列会自动丢弃最早的事件。
 * @tparam T
 */
template <typename T>
class RingBuffer {
public:
    RingBuffer(size_t size);
    ~RingBuffer();
    ///> 压入一个事件队尾
    auto push(const T &event) -> void;
    ///> move一个事件到队尾
    auto push(T &&event) -> void;
    ///> 就地构造一个事件到队尾
    template <typename... Args>
    auto emplace(Args &&...args) -> void;
    ///> 弹出一个队首的事件
    auto pop(T &event) -> bool;
    ///> 返回存在多少事件
    auto size() -> std::size_t;

private:
    std::unique_ptr<T[]> _buffer   = nullptr;
    std::size_t          _head     = 0;
    std::size_t          _tail     = 0;
    std::size_t          _capacity = 0;
};

template <typename T>
RingBuffer<T>::RingBuffer(size_t size)
{
    _buffer.reset(new T[size + 1]);
    _capacity = size + 1;
}

template <typename T>
RingBuffer<T>::~RingBuffer()
{
}

template <typename T>
auto RingBuffer<T>::push(const T &event) -> void
{
    _buffer[_head] = event;
    _head          = (_head + 1) % _capacity;
    if (_head == _tail) {
        _tail = (_tail + 1) % _capacity;
    }
}

template <typename T>
auto RingBuffer<T>::push(T &&event) -> void
{
    _buffer[_head] = std::move(event);
    _head          = (_head + 1) % _capacity;
    if (_head == _tail) {
        _tail = (_tail + 1) % _capacity;
    }
}

template <typename T>
template <typename... Args>
auto RingBuffer<T>::emplace(Args &&...args) -> void
{
    _buffer[_head] = std::move(T{std::forward<Args>(args)...});
    _head          = (_head + 1) % _capacity;
    if (_head == _tail) {
        _tail = (_tail + 1) % _capacity;
    }
}

template <typename T>
auto RingBuffer<T>::pop(T &event) -> bool
{
    if (_head == _tail) {
        return false;
    }
    event = std::move(_buffer[_tail]);
    _tail = (_tail + 1) % _capacity;
    return true;
}

template <typename T>
auto RingBuffer<T>::size() -> std::size_t
{
    return (_head - _tail + _capacity) % _capacity;
}

class EventListener {
public:
    EventListener();
    ~EventListener();
    ///> 获取一个事件，如果没有就等待
    auto get_event() -> Task<NekoProto::IProto>;

private:
    LRESULT _mouse_proc(int ncode, WPARAM wp, LPARAM lp);
    LRESULT _keyboard_proc(int ncode, WPARAM wp, LPARAM lp);
    ///> 唤起正在等待事件的协程
    auto _notify() -> void;

    HHOOK    _mosueHook;
    HHOOK    _keyboardHook;
    uint64_t _screenWidth;
    uint64_t _screenHeight;

    RingBuffer<NekoProto::IProto> _events;
    ILIAS_NAMESPACE::Event        _syncEvent;
};

auto EventListener::get_event() -> Task<NekoProto::IProto>
{
    if (_events.size() == 0) {
        _syncEvent.clear();
        co_await _syncEvent;
    }
    NekoProto::IProto proto;
    _events.pop(proto);
    co_return std::move(proto);
}

auto EventListener::_notify() -> void
{
    _syncEvent.set();
}

EventListener::EventListener() : _events(10)
{
    static EventListener *self = nullptr;
    self                       = this;
    _mosueHook                 = SetWindowsHookExW(
        WH_MOUSE_LL,
        [](int ncode, WPARAM wp, LPARAM lp) { return self->_mouse_proc(ncode, wp, lp); }, nullptr,
        0);
    if (_mosueHook == nullptr) {
        std::cout << "SetWindowsHookExW failed: " << GetLastError() << std::endl;
        return;
    }

    _keyboardHook = SetWindowsHookExW(
        WH_KEYBOARD_LL,
        [](int ncode, WPARAM wp, LPARAM lp) { return self->_keyboard_proc(ncode, wp, lp); },
        nullptr, 0);
    if (_keyboardHook == nullptr) {
        std::cout << "SetWindowsHookExW failed: " << GetLastError() << std::endl;
        return;
    }
    _screenWidth  = GetSystemMetrics(SM_CXSCREEN);
    _screenHeight = GetSystemMetrics(SM_CYSCREEN);
}

EventListener::~EventListener()
{
    UnhookWindowsHookEx(_mosueHook);
    UnhookWindowsHookEx(_keyboardHook);
}

LRESULT EventListener::_mouse_proc(int ncode, WPARAM wp, LPARAM lp)
{
    auto             *hookStruct = reinterpret_cast<MSLLHOOKSTRUCT *>(lp);
    NekoProto::IProto proto;
    switch (wp) {
    case WM_LBUTTONDOWN:
        proto = mks::MouseButtonEvent::emplaceProto(
            mks::MouseButtonState::eButtonDown, mks::MouseButton::eButtonLeft, (uint8_t)1,
            (uint64_t)std::chrono::system_clock::now().time_since_epoch().count());
        std::cout << "Left Mouse Button Down" << std::endl;
        break;
    case WM_LBUTTONUP:
        proto = mks::MouseButtonEvent::emplaceProto(
            mks::MouseButtonState::eButtonUp, mks::MouseButton::eButtonLeft, (uint8_t)1,
            (uint64_t)std::chrono::system_clock::now().time_since_epoch().count());
        std::cout << "Left Mouse Button Up" << std::endl;
        break;
    case WM_LBUTTONDBLCLK:
        proto = mks::MouseButtonEvent::emplaceProto(
            mks::MouseButtonState::eClick, mks::MouseButton::eButtonLeft, (uint8_t)2,
            (uint64_t)std::chrono::system_clock::now().time_since_epoch().count());
        break;
    case WM_RBUTTONDOWN:
        proto = mks::MouseButtonEvent::emplaceProto(
            mks::MouseButtonState::eButtonDown, mks::MouseButton::eButtonRight, (uint8_t)1,
            (uint64_t)std::chrono::system_clock::now().time_since_epoch().count());
        std::cout << "Right Mouse Button Down" << std::endl;
        break;
    case WM_RBUTTONUP:
        proto = mks::MouseButtonEvent::emplaceProto(
            mks::MouseButtonState::eButtonUp, mks::MouseButton::eButtonRight, (uint8_t)1,
            (uint64_t)std::chrono::system_clock::now().time_since_epoch().count());
        std::cout << "Right Mouse Button Up" << std::endl;
        break;
    case WM_RBUTTONDBLCLK:
        proto = mks::MouseButtonEvent::emplaceProto(
            mks::MouseButtonState::eClick, mks::MouseButton::eButtonRight, (uint8_t)2,
            (uint64_t)std::chrono::system_clock::now().time_since_epoch().count());
        break;
    case WM_MBUTTONDOWN:
        proto = mks::MouseButtonEvent::emplaceProto(
            mks::MouseButtonState::eButtonDown, mks::MouseButton::eButtonMiddle, (uint8_t)1,
            (uint64_t)std::chrono::system_clock::now().time_since_epoch().count());
        break;
    case WM_MBUTTONUP:
        proto = mks::MouseButtonEvent::emplaceProto(
            mks::MouseButtonState::eButtonUp, mks::MouseButton::eButtonMiddle, (uint8_t)1,
            (uint64_t)std::chrono::system_clock::now().time_since_epoch().count());
        break;
    case WM_MBUTTONDBLCLK:
        proto = mks::MouseButtonEvent::emplaceProto(
            mks::MouseButtonState::eClick, mks::MouseButton::eButtonMiddle, (uint8_t)2,
            (uint64_t)std::chrono::system_clock::now().time_since_epoch().count());
        break;
    case WM_MOUSEMOVE:
        proto = mks::MouseMotionEvent::emplaceProto(
            (float)hookStruct->pt.x / _screenWidth, (float)hookStruct->pt.y / _screenHeight,
            (uint64_t)std::chrono::system_clock::now().time_since_epoch().count());
        std::cout << "Mouse Move X: " << hookStruct->pt.x << " Y: " << hookStruct->pt.y
                  << std::endl;
        break;
    case WM_MOUSEWHEEL:
        proto = mks::MouseWheelEvent::emplaceProto(
            (float)hookStruct->mouseData / WHEEL_DELTA, 0.F,
            (uint64_t)std::chrono::system_clock::now().time_since_epoch().count());
        std::cout << "Mouse Wheel Delta: " << HIWORD(hookStruct->mouseData) << std::endl;
        break;
    default:
        return CallNextHookEx(nullptr, ncode, wp, lp);
    }
    _events.push(std::move(proto));
    _syncEvent.set();
    return CallNextHookEx(nullptr, ncode, wp, lp);
}

LRESULT EventListener::_keyboard_proc(int ncode, WPARAM wp, LPARAM lp)
{
    auto *hookStruct = reinterpret_cast<KBDLLHOOKSTRUCT *>(lp);
    switch (wp) {
    case WM_KEYDOWN:
        std::cout << "Key Down ";
        break;
    case WM_KEYUP:
        std::cout << "Key Up ";
    }
    char name[256]{0};
    GetKeyNameTextA(hookStruct->scanCode << 16, name, 256);
    uint16_t rawcode;
    bool     virtualKey;
    auto     keycode = mks::windows_scan_code_to_key_code(hookStruct->scanCode, hookStruct->vkCode,
                                                          &rawcode, &virtualKey);

    mks::KeyMod keymod = mks::KeyMod::eKmodNone;
    if ((GetAsyncKeyState(VK_LMENU) & 0x8000) != 0) {
        keymod = mks::KeyMod::eKmodLalt;
    }
    if ((GetAsyncKeyState(VK_RMENU) & 0x8000) != 0) {
        keymod = mks::KeyMod::eKmodRalt;
    }
    if ((GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0) {
        keymod = mks::KeyMod::eKmodLctrl;
    }
    if ((GetAsyncKeyState(VK_RCONTROL) & 0x8000) != 0) {
        keymod = mks::KeyMod::eKmodRctrl;
    }
    if ((GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0) {
        keymod = mks::KeyMod::eKmodShift;
    }
    if ((GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0) {
        keymod = mks::KeyMod::eKmodRshift;
    }
    if ((GetAsyncKeyState(VK_LWIN) & 0x8000) != 0) {
        keymod = mks::KeyMod::eKmodLgui;
    }
    if ((GetAsyncKeyState(VK_RWIN) & 0x8000) != 0) {
        keymod = mks::KeyMod::eKmodRgui;
    }
    if ((GetAsyncKeyState(VK_CAPITAL) & 0x0001) != 0) {
        keymod = mks::KeyMod::eKmodCaps;
    }
    if ((GetAsyncKeyState(VK_NUMLOCK) & 0x0001) != 0) {
        keymod = mks::KeyMod::eKmodNum;
    }
    if ((GetAsyncKeyState(VK_SCROLL) & 0x0001) != 0) {
        keymod = mks::KeyMod::eKmodScroll;
    }

    _events.emplace(mks::KeyEvent::emplaceProto(
        (wp == WM_KEYDOWN) ? mks::KeyboardState::eKeyDown : mks::KeyboardState::eKeyUp, keycode,
        keymod, (uint64_t)std::chrono::system_clock::now().time_since_epoch().count()));
    _syncEvent.set();
    return CallNextHookEx(nullptr, ncode, wp, lp);
}

/**
 * @brief event sender
 * 系统事件构造对象，用于将接收的事件构造并发送给系统。
 *
 */
class EventSender {
public:
    EventSender();
    ~EventSender();

    void send_motion_event(const mks::MouseMotionEvent &event) const;
    void send_button_event(const mks::MouseButtonEvent &event) const;
    void send_wheel_event(const mks::MouseWheelEvent &event) const;
    void send_keyboard_event(const mks::KeyEvent &event) const;

private:
    uint64_t _screenWidth;
    uint64_t _screenHeight;
};

EventSender::EventSender()
{
    _screenWidth  = GetSystemMetrics(SM_CXSCREEN);
    _screenHeight = GetSystemMetrics(SM_CYSCREEN);
}

EventSender::~EventSender() {}

void EventSender::send_motion_event(const mks::MouseMotionEvent &event) const
{
    INPUT input          = {0};
    input.type           = INPUT_MOUSE;
    input.mi.dx          = (LONG)(event.x * _screenWidth);
    input.mi.dy          = (LONG)(event.y * _screenHeight);
    input.mi.mouseData   = 0;
    input.mi.dwFlags     = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    input.mi.time        = 0;
    input.mi.dwExtraInfo = 0;
    SendInput(1, &input, sizeof(INPUT));
}

void EventSender::send_button_event(const mks::MouseButtonEvent &event) const
{
    std::unique_ptr<INPUT[]> input;
    int                      count = 1;
    if (event.clicks <= 1) {
        input = std::make_unique<INPUT[]>(0);
    }
    else {
        count = event.clicks;
        input = std::make_unique<INPUT[]>(event.clicks);
    }
    for (int i = 0; i < count; ++i) {
        memset(&input[i], 0, sizeof(INPUT));
        input[i].type       = INPUT_MOUSE;
        input[i].mi.dwFlags = 0;
        input[i].mi.time    = 0;
        switch (event.button) {
        case mks::MouseButton::eButtonLeft:
            input[i].mi.dwFlags =
                (event.state == mks::MouseButtonState::eButtonUp ? MOUSEEVENTF_LEFTUP
                                                                 : MOUSEEVENTF_LEFTDOWN);
            break;
        case mks::MouseButton::eButtonRight:
            input[i].mi.dwFlags =
                (event.state == mks::MouseButtonState::eButtonUp ? MOUSEEVENTF_RIGHTUP
                                                                 : MOUSEEVENTF_RIGHTDOWN);
            break;
        case mks::MouseButton::eButtonMiddle:
            input[i].mi.dwFlags =
                (event.state == mks::MouseButtonState::eButtonUp ? MOUSEEVENTF_MIDDLEDOWN
                                                                 : MOUSEEVENTF_MIDDLEUP);
            break;
        case mks::MouseButton::eButtonX1:
            input[i].mi.mouseData = XBUTTON1;
        case mks::MouseButton::eButtonX2:
            input[i].mi.mouseData = XBUTTON2;
            input[i].mi.dwFlags =
                (event.state == mks::MouseButtonState::eButtonUp ? MOUSEEVENTF_XUP
                                                                 : MOUSEEVENTF_XDOWN);
            break;
        default:
            break;
        }
    }
    SendInput(count, input.get(), sizeof(INPUT));
}

void EventSender::send_wheel_event(const mks::MouseWheelEvent &event) const
{
    INPUT input          = {0};
    input.type           = INPUT_MOUSE;
    input.mi.dx          = 0;
    input.mi.dy          = 0;
    input.mi.mouseData   = (LONG)(event.x * WHEEL_DELTA);
    input.mi.dwFlags     = MOUSEEVENTF_WHEEL;
    input.mi.time        = 0;
    input.mi.dwExtraInfo = 0;
    SendInput(1, &input, sizeof(INPUT));
}

void EventSender::send_keyboard_event(const mks::KeyEvent &event) const
{
    INPUT input          = {0};
    input.type           = INPUT_KEYBOARD;
    input.ki.wVk         = 0;
    input.ki.wScan       = (WORD)key_code_to_windows_scan_code(event.key);
    input.ki.time        = 0;
    input.ki.dwExtraInfo = 0;
    input.ki.dwFlags =
        (event.state == mks::KeyboardState::eKeyUp ? KEYEVENTF_KEYUP : 0) | KEYEVENTF_SCANCODE;
    if ((input.ki.wScan & 0xe000) != 0) {
        input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    }
    SendInput(1, &input, sizeof(INPUT));
}

struct CommandsData {
    std::vector<std::string> command;
    std::string              description;
    std::function<void(const std::vector<std::string>           &args,
                       const std::map<std::string, std::string> &options)>
        callback;
};

class App {
public:
    App();
    ~App();

    static auto app_name() -> std::string;
    auto        install_command(const CommandsData &command) -> void;

    auto exec() -> Task<void>;
    auto stop() -> void;

    // server
    /**
     * @brief start server
     * 如果需要ilias_go参数列表必须复制。
     * @param endpoint
     * @return Task<void>
     */
    auto start_server(IPEndpoint endpoint) -> Task<void>;
    auto stop_server() -> void;
    auto start_capture() -> Task<void>;
    auto stop_capture() -> void;

    // client
    /**
     * @brief connect to server
     * 如果需要ilias_go参数列表必须复制。
     * @param endpoint
     * @return Task<void>
     */
    auto connect_to(IPEndpoint endpoint) -> Task<void>;
    auto disconnect() -> void;

    // commands
    auto start_console() -> Task<void>;
    auto stop_console() -> void;

    static auto split(std::string_view str, char ch = ' ') -> std::vector<std::string_view>;

private:
    // for server
    auto _accept_client(TcpClient client) -> Task<void>;

private:
    bool _isClienting        = false;
    bool _isServering        = false;
    bool _isCapturing        = false;
    bool _isRuning           = false;
    bool _isConsoleListening = false;

    std::unique_ptr<EventListener>                  _listener;
    std::unique_ptr<EventSender>                    _eventSender;
    std::unique_ptr<NekoProto::ProtoStreamClient<>> _protoStreamClient;
    NekoProto::ProtoFactory                         _protofactory;
    std::vector<CommandsData>                       _commands;
};

App::App()
{
    // 注册帮助命令
    install_command({
        {"help", "h", "?"},
        "show helps",
        [this](const std::vector<std::string> &  /*unused*/,
               const std::map<std::string, std::string> &  /*unused*/) {
            printf("%s [command] [args] [options]\n", app_name().c_str());
            for (const auto &command : _commands) {
                printf("    %s", command.command[0].c_str());
                if (command.command.size() > 1) {
                    printf("(");
                    for (size_t i = 1; i < command.command.size(); ++i) {
                        printf("%s", command.command[i].c_str());
                        if (i != command.command.size() - 1) {
                            printf(", ");
                        }
                    }
                    printf(")");
                }
                printf(":\n        %s\n", command.description.c_str());
            }
         }
    });
    // 注册启动服务命令
    install_command({
        {"start", "s"},
        "start server, e.g. start 127.0.0.1 12345",
        [this](const std::vector<std::string>           &args,
               const std::map<std::string, std::string> &options) {
            IPEndpoint ipendpoint("127.0.0.1:12345");
            if (args.size() == 2) {
                auto ret = IPEndpoint::fromString(args[0] + ":" + args[1]);
                if (ret) {
                    ipendpoint = ret.value();
                }
                else {
                    spdlog::error("ip endpoint error {}", ret.error().message());
                }
            }
            else if (args.size() == 1) {
                auto ret = IPEndpoint::fromString(args[0]);
                if (ret) {
                    ipendpoint = ret.value();
                }
                else {
                    spdlog::error("ip endpoint error {}", ret.error().message());
                }
            }
            else {
                auto it      = options.find("address");
                auto address = it == options.end() ? "127.0.0.1" : it->second;
                it           = options.find("port");
                auto port    = it == options.end() ? "12345" : it->second;
                auto ret     = IPEndpoint::fromString(address + ":" + port);
                if (ret) {
                    ipendpoint = ret.value();
                }
                else {
                    spdlog::error("ip endpoint error {}", ret.error().message());
                }
            }
            ilias_go start_server(ipendpoint);
         }
    });
    // 注册开始捕获键鼠事件命令
    install_command({
        {"capture", "c"},
        "start keyboard/mouse capture",
        [this](const std::vector<std::string> &  /*unused*/,
               const std::map<std::string, std::string> &  /*unused*/) {
            ilias_go start_capture();
         }
    });
    // 注册退出程序命令
    install_command({
        {"stop", "quit", "q"},
        "stop keyboard/mouse capture",
        [this](const std::vector<std::string> &  /*unused*/,
               const std::map<std::string, std::string> &  /*unused*/) { stop(); }
    });
    // 注册连接到服务器命令
    install_command({{"connect"},
                     "connect to server, e.g. connect 127.0.0.1 12345",
                     [this](const std::vector<std::string>           &args,
                            const std::map<std::string, std::string> &options) {
                         if (args.size() == 2) {
                             ilias_go connect_to(IPEndpoint(args[0] + ":" + args[1]));
                         }
                         else if (args.size() == 1) {
                             ilias_go connect_to(IPEndpoint(args[0]));
                         }
                         else {
                             auto it       = options.find("address");
                             auto address  = it == options.end() ? "127.0.0.1" : it->second;
                             it            = options.find("port");
                             auto     port = it == options.end() ? "12345" : it->second;
                             ilias_go connect_to(IPEndpoint(address + ":" + port));
                         }
                     }});
}

App::~App() {}

auto App::app_name() -> std::string
{
    return "mksync";
}

auto App::install_command(const CommandsData &command) -> void
{
    _commands.push_back(command);
}

auto App::connect_to(IPEndpoint endpoint) -> Task<void>
{
    if (_isServering) { // 确保没有启动服务模式
        spdlog::error("please quit server mode first");
        co_return;
    }
    if (_eventSender) { // 确保当前没有正在作为客户端运行
        co_return;
    }
    _eventSender = std::make_unique<EventSender>();
    auto ret     = co_await TcpClient::make(AF_INET);
    if (!ret) {
        ILIAS_ERROR("mksync-test", "TcpClient::make failed {}", ret.error().message());
        co_return;
    }
    auto tcpClient = std::move(ret.value());
    _isClienting   = true;
    auto ret1      = co_await tcpClient.connect(endpoint);
    if (!ret1) {
        ILIAS_ERROR("mksync-test", "TcpClient::connect failed {}", ret1.error().message());
        co_return;
    }
    _protoStreamClient = // 构造用于传输协议的壳
        std::make_unique<NekoProto::ProtoStreamClient<>>(_protofactory, std::move(tcpClient));
    _isClienting = true;
    while (_isClienting) {
        auto ret2 = co_await _protoStreamClient->recv();
        if (!ret2) {
            ILIAS_ERROR("mksync-test", "ProtoStreamClient::recv failed {}", ret2.error().message());
            disconnect();
            co_return;
        }
        auto msg = std::move(ret2.value());
        if (_eventSender) {
            if (msg.type() == NekoProto::ProtoFactory::protoType<mks::KeyEvent>()) {
                _eventSender->send_keyboard_event(*msg.cast<mks::KeyEvent>());
            }
            else if (msg.type() == NekoProto::ProtoFactory::protoType<mks::MouseButtonEvent>()) {
                _eventSender->send_button_event(*msg.cast<mks::MouseButtonEvent>());
            }
            else if (msg.type() == NekoProto::ProtoFactory::protoType<mks::MouseMotionEvent>()) {
                _eventSender->send_motion_event(*msg.cast<mks::MouseMotionEvent>());
            }
            else if (msg.type() == NekoProto::ProtoFactory::protoType<mks::MouseWheelEvent>()) {
                _eventSender->send_wheel_event(*msg.cast<mks::MouseWheelEvent>());
            }
            else {
                ILIAS_ERROR("mksync-test", "unused message type {}", msg.protoName());
            }
        }
    }
    co_return;
}

auto App::disconnect() -> void
{
    _isClienting = false;
    _eventSender.reset();
    if (_protoStreamClient) {
        _protoStreamClient->close().wait();
    }
    _eventSender.reset();
}

auto App::start_server(IPEndpoint endpoint) -> Task<void>
{
    if (_isClienting) { // 确保没有作为客户端模式运行
        spdlog::error("please quit client mode first");
        co_return;
    }
    if (_isServering) { // 确保没有正在作为服务端运行
        spdlog::error("server is already running");
        co_return;
    }
    auto ret = co_await TcpListener::make(AF_INET);
    if (!ret) {
        spdlog::error("TcpListener::make failed {}", ret.error().message());
        co_return;
    }
    spdlog::info("start server with {}", endpoint.toString());
    auto tcpServer = std::move(ret.value());
    auto ret2      = tcpServer.bind(endpoint);
    if (!ret2) {
        spdlog::error("TcpListener::bind failed {}", ret2.error().message());
    }
    _isServering = true;
    while (_isServering) {
        auto ret1 = co_await tcpServer.accept();
        if (!ret1) {
            ILIAS_ERROR("mksync-test", "TcpListener::accept failed {}", ret1.error().message());
            stop_server();
            co_return;
        }
        auto     tcpClient = std::move(ret1.value());
        ilias_go _accept_client(std::move(tcpClient.first));
    }
}

auto App::stop_server() -> void
{
    _isServering = false;
}

auto App::split(std::string_view str, char ch) -> std::vector<std::string_view>
{ // 拆分字符串,像"a b c d e"到{"a", "b", "c", "d", "e"}。
    std::vector<std::string_view> res;
    std::string_view              sv(str);
    while (!sv.empty()) {
        auto pos = sv.find(ch);
        if (pos != std::string_view::npos) {
            res.emplace_back(sv.substr(0, pos));
            sv.remove_prefix(pos + 1);
        }
        else {
            res.emplace_back(sv);
            sv.remove_prefix(sv.size());
        }
    }
    return res;
}

auto App::start_console() -> Task<void>
{                              // 监听终端输入
    if (_isConsoleListening) { // 确保没有正在监听中。
        spdlog::error("console is already listening");
        co_return;
    }
    auto ret = co_await ILIAS_NAMESPACE::Console::fromStdin();
    if (!ret) {
        ILIAS_ERROR("mksync-test", "Console::fromStdin failed {}", ret.error().message());
        co_return;
    }
    ILIAS_NAMESPACE::Console console       = std::move(ret.value());
    _isConsoleListening                    = true;
    std::unique_ptr<std::byte[]> strBuffer = std::make_unique<std::byte[]>(1024);
    while (_isConsoleListening) {
        memset(strBuffer.get(), 0, 1024);
        auto ret1 = co_await console.read({strBuffer.get(), 1024});
        if (!ret1) {
            ILIAS_ERROR("mksync-test", "Console::read failed {}", ret1.error().message());
            stop_console();
            co_return;
        }
        auto            *line = reinterpret_cast<char *>(strBuffer.get());
        std::string_view lineView(line);
        while (lineView.size() > 0 && (lineView.back() == '\r' || lineView.back() == '\n')) {
            lineView.remove_suffix(1);
        }
        auto splited = split(lineView);
        if (splited.size() == 0) {
            continue;
        }
        std::vector<std::string>           args;
        std::map<std::string, std::string> options;
        auto                               cmd = splited.front();
        for (int i = 1; i < (int)splited.size(); ++i) { // 将命令的选项与参数提取出来
            auto str = splited[i];
            if (str[0] == '-') {
                auto pos = str.find('=');
                if (pos != std::string::npos) {
                    options[std::string(str.substr(1, pos - 1))] = std::string(str.substr(pos + 1));
                }
                else {
                    options[std::string(str.substr(1))] = "";
                }
            }
            else {
                args.emplace_back(std::string(str));
            }
        }
        spdlog::info("command: {}, args: {}, options: {}", cmd, args, options);
        for (auto command : _commands) { // TODO: 查找命令并执行, 这里的字符匹配，可以优化。
            if (std::ranges::any_of(
                    command.command.begin(), command.command.end(),
                    [&cmd](const std::string &command) { return command == cmd; })) {
                command.callback(args, options);
                break;
            }
        }
    }
}

auto App::stop_console() -> void
{
    _isConsoleListening = false;
}

auto App::_accept_client(TcpClient client) -> Task<void>
{ // 服务端处理客户连接
    spdlog::debug("accept client {}", client.remoteEndpoint()->toString());
    // do some thing for client
    _protoStreamClient =
        std::make_unique<NekoProto::ProtoStreamClient<>>(_protofactory, std::move(client));
    co_return;
}

/**
 * @brief
 * server:
 * 1. start server(启动网络监听)
 * 2. start capture 启动键鼠捕获
 * client
 * 1. connect to server 连接服务端
 * @return Task<void>
 */
auto App::exec() -> Task<void>
{
    _isRuning = true;
    while (_isRuning) {
        co_await start_console();
    }
    co_return;
}

auto App::stop() -> void
{
    _isRuning = false;
    stop_server();
    stop_capture();
    disconnect();
    stop_console();
}

auto App::start_capture() -> Task<void>
{
    if (_isClienting || !_isServering || !_isRuning) { // 确保是作为服务端运行中
        spdlog::error("please start capture in server mode");
        co_return;
    }
    spdlog::debug("start_capture");
    if (_listener && _isCapturing) { // 已经启动了
        co_return;
    }
    _isCapturing = true;
    _listener    = std::make_unique<EventListener>();
    while (_isCapturing) {
        NekoProto::IProto proto = (co_await _listener->get_event());
        spdlog::debug("protoName {}", proto.protoName());
        if (_protoStreamClient) {
            auto ret2 = co_await _protoStreamClient->send(proto);
            if (!ret2) {
                ILIAS_ERROR("mksync-test", "send proto failed {}", ret2.error().message());
                stop_capture();
                co_return;
            }
        }
    }
}

auto App::stop_capture() -> void
{
    _isCapturing = false;
    _listener.reset();
}

int main()
{

    ILIAS_LOG_SET_LEVEL(ILIAS_INFO_LEVEL);
    spdlog::set_level(spdlog::level::debug);
    WinContext context;
    App        app;
    // NekoProto::ProtoStreamClient<> d(protofactory, "127.0.0.1", 1234);
    ilias_wait app.exec();
    return 0;
}