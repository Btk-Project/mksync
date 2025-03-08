#pragma once
// sudo apt-get install libxcb1-dev libxcb-util0-dev
#include <sys/mman.h>
#include <iostream>
#include <ilias/ilias.hpp>
#include <ilias/fs/console.hpp>
#include <ilias/fs/file.hpp>
#include <ilias/sync/scope.hpp>
#include <ilias/sync/oneshot.hpp>
#include <ilias/platform.hpp>

#include <set>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_event.h>
#include <xcb/xcb_util.h>
#include <X11/Xutil.h>

#define XCB_NAMESPACE xcbspace
#define XCB_NAMESPACE_BEGIN                                                                        \
    namespace XCB_NAMESPACE                                                                        \
    {
#define XCB_NAMESPACE_END }

XCB_NAMESPACE_BEGIN

using ILIAS_NAMESPACE::Error;
using ILIAS_NAMESPACE::File;
using ILIAS_NAMESPACE::IoContext;
using ILIAS_NAMESPACE::IoDescriptor;
using ILIAS_NAMESPACE::PlatformContext;
using ILIAS_NAMESPACE::PollEvent;
using ILIAS_NAMESPACE::Task;
using ILIAS_NAMESPACE::IoTask;
using ILIAS_NAMESPACE::Unexpected;
using ILIAS_NAMESPACE::oneshot::channel;
using ILIAS_NAMESPACE::oneshot::Receiver;
using ILIAS_NAMESPACE::oneshot::Sender;

class XcbConnect;

class XcbWindow {
public:
    XcbWindow(XcbConnect *conn);
    XcbWindow(XcbConnect *conn, xcb_window_t window, bool destroyAble = false);

    ~XcbWindow();

    void show();
    void hide();
    void destroy();

    int grab_keyboard(bool grab, bool ownerEvents = false);
    int grab_pointer(bool grab, bool ownerEvents = false);

    auto set_geometry(int posx, int posy, int width, int height) -> void;
    auto event_loop(std::unique_ptr<xcb_generic_event_t> event) -> IoTask<void>;
    auto set_property(const std::string &name, const std::string &value) -> int;
    auto set_attribute(uint32_t eventMask /* xcb_event_mask_t*/, uint32_t values[] /* value list */)
        -> int;
    auto set_transparent(uint32_t alpha) -> void;
    auto native_handle() const -> xcb_window_t;

private:
    auto _create_window() -> void;

private:
    xcb_window_t          _window          = XCB_WINDOW_NONE;
    XcbConnect           *_conn            = nullptr;
    bool                  _grabbedKeyboard = false;
    bool                  _grabbedPointer  = false;
    bool                  _destroyAble     = true;
};

class XcbConnect {
public:
    XcbConnect(IoContext *ctx)
    {
        if (ctx != nullptr) {
            _context = ctx;
        }
        else {
            _context = PlatformContext::currentThread();
        }
    }
    ~XcbConnect() { disconnect(); }

    auto connect(const char *displayname, int *screenp) -> IoTask<void>;
    auto disconnect() -> void;
    auto get_default_screen() -> xcb_screen_t *;
    auto get_default_root_window() -> XcbWindow;

    auto event_loop() -> IoTask<void>;
    auto fake_key_press(xcb_keycode_t keycode) -> IoTask<void>;
    auto fake_key_release(xcb_keycode_t keycode) -> IoTask<void>;
    auto fake_mouse_move(int16_t rootX, int16_t rootY) -> IoTask<void>;
    auto fake_mouse_button_press(xcb_button_t button) -> IoTask<void>;
    auto fake_mouse_button_release(xcb_button_t button) -> IoTask<void>;
    auto event_dispatcher(std::unique_ptr<xcb_generic_event_t> event) -> IoTask<void>;
    auto grab_pointer(XcbWindow *window, bool owner = false) -> int;
    auto ungrab_pointer() -> int;
    auto grab_keyboard(XcbWindow *window, bool owner = false) -> int;
    auto ungrab_keyboard() -> int;

    xcb_atom_t get_atom(const char *name);

    int               generate_id();
    xcb_connection_t *connection() { return _connection; }
    int               flush()
    {
        if (_connection != nullptr) {
            return xcb_flush(_connection);
        }
        return -1;
    }
    xcb_key_symbols_t *key_symbols() { return _keySymbols.get(); }
    void               add_root_window(XcbWindow *window) { _windows.insert(window); }
    void               remove_root_window(XcbWindow *window) { _windows.erase(window); }

private:
    auto _init_io_descriptor() -> void;
    auto _destroy_io_descriptor() -> void;
    auto _poll_event() -> IoTask<void>;

private:
    xcb_connection_t *_connection = nullptr;
    Display          *_display    = nullptr;
    IoDescriptor     *_fd         = nullptr;
    IoContext        *_context    = nullptr;
    std::unique_ptr<xcb_key_symbols_t, std::function<void(xcb_key_symbols_t *)>>       _keySymbols;
    std::unordered_map<int, std::function<void(std::unique_ptr<xcb_generic_event_t>)>> _sequeueMap;
    std::set<XcbWindow *>                                                              _windows;
};

inline IoTask<void> example()
{
    // 打开 X 连接
    XcbConnect xcb(PlatformContext::currentThread());
    int        screenNumber;
    auto       ret = co_await xcb.connect(nullptr, &screenNumber);
    if (!ret) {
        std::cerr << "Failed to connect to X server: " << ret.error().message() << std::endl;
        co_return Unexpected<Error>(ret.error());
    }

    // 创建窗口
    // xcb_window_t window   = xcb.generate_id();
    XcbWindow window(&xcb);
    window.set_property("WM_TYPE", "NORMAL"); // 设置窗口类型

    // 显示窗口
    window.show();
    xcb_flush(xcb.connection());

    // 事件循环
    std::unique_ptr<xcb_generic_event_t> event;
    std::cout << "Press 'q' or 'Esc' to exit" << std::endl;
    auto ret1 = co_await xcb.event_loop();
    co_return ret1;
}

XCB_NAMESPACE_END