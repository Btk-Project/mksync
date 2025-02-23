/**
 * @file xcb_window.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-02-23
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once
#ifdef __linux__
    // sudo apt-get install libxcb1-dev libxcb-util0-dev
    #include <sys/mman.h>
    #include <ilias/ilias.hpp>
    #include <ilias/fs/console.hpp>
    #include <ilias/sync/scope.hpp>
    #include <ilias/sync/oneshot.hpp>
    #include <ilias/platform.hpp>

    #include <xcb/xcb.h>
    #include <xcb/xcb_keysyms.h>
    #include <xcb/xcb_event.h>
    #include <xcb/xcb_util.h>
    #include <X11/Xutil.h>

namespace mks::base
{
    using ilias::Error;
    using ilias::IoContext;
    using ilias::IoDescriptor;
    using ilias::IoTask;
    using ilias::PlatformContext;
    using ilias::PollEvent;
    using ilias::Task;
    using ilias::Unexpected;
    using ilias::oneshot::channel;
    using ilias::oneshot::Receiver;
    using ilias::oneshot::Sender;

    class XcbConnect;

    class XcbWindow {
    public:
        XcbWindow(XcbConnect *conn);
        XcbWindow(XcbConnect *conn, xcb_window_t window, bool destroyAble = false);

        ~XcbWindow();

        void show();
        void hide();
        void destroy();

        auto set_geometry(int posx, int posy, int width, int height) -> void;
        auto set_property(const std::string &name, const std::string &value) -> int;
        auto set_attribute(uint32_t eventMask /* xcb_event_mask_t*/,
                           uint32_t values[] /* value list */) -> int;
        auto set_transparent(uint32_t alpha) -> void;
        auto native_handle() const -> xcb_window_t;

    private:
        auto _create_window() -> void;

    private:
        xcb_window_t _window          = XCB_WINDOW_NONE;
        XcbConnect  *_conn            = nullptr;
        bool         _grabbedKeyboard = false;
        bool         _grabbedPointer  = false;
        bool         _destroyAble     = true;
    };

    class XcbConnect {
    public:
        XcbConnect(::ilias::IoContext *ctx)
        {
            if (ctx != nullptr) {
                _context = ctx;
            }
            else {
                _context = IoContext::currentThread();
            }
        }
        ~XcbConnect() { disconnect(); }

        auto connect(const char *displayname) -> IoTask<int>;
        auto disconnect() -> void;
        auto get_default_screen() -> xcb_screen_t *;
        auto get_default_root_window() -> XcbWindow;

        auto event_loop() -> IoTask<void>;
        auto send_key_press(xcb_keycode_t keycode) -> IoTask<void>;
        auto send_key_release(xcb_keycode_t keycode) -> IoTask<void>;
        auto send_mouse_move(int16_t rootX, int16_t rootY) -> IoTask<void>;
        auto send_mouse_button_press(xcb_button_t button) -> IoTask<void>;
        auto send_mouse_button_release(xcb_button_t button) -> IoTask<void>;
        auto grab_pointer(XcbWindow *window, std::function<void(xcb_generic_event_t *)> callback,
                          bool owner = false) -> int;
        auto ungrab_pointer() -> int;
        auto grab_keyboard(XcbWindow *window, std::function<void(xcb_generic_event_t *)> callback,
                           bool owner = false) -> int;
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

    private:
        auto _init_io_descriptor() -> void;
        auto _destroy_io_descriptor() -> void;
        auto _poll_event() -> IoTask<void>;

    private:
        xcb_connection_t   *_connection = nullptr;
        IoDescriptor       *_fd         = nullptr;
        ::ilias::IoContext *_context    = nullptr;
        std::unique_ptr<xcb_key_symbols_t, std::function<void(xcb_key_symbols_t *)>> _keySymbols;
        std::unordered_map<int, std::function<void(std::unique_ptr<xcb_generic_event_t>)>>
                                                   _sequeueMap;
        std::function<void(xcb_generic_event_t *)> _pointerCallback;
        std::function<void(xcb_generic_event_t *)> _keyboardCallback;
    };
} // namespace mks::base
#endif