#include "mksync/base/platform/xcb_window.hpp"
#ifdef __linux__
    #include <spdlog/spdlog.h>
    #include <xcb/xtest.h>

namespace mks::base
{

    int XcbConnect::generate_id()
    {
        return xcb_generate_id(_connection);
    }

    int XcbConnect::flush()
    {
        if (_connection != nullptr) {
            return xcb_flush(_connection);
        }
        return -1;
    }

    auto XcbConnect::get_keyboard_map() -> std::unordered_map<uint32_t, xcb_keysym_t>
    {
        std::unordered_map<uint32_t, xcb_keysym_t> keysymsMap;
        const xcb_setup_t                         *setup           = xcb_get_setup(_connection);
        xcb_get_keyboard_mapping_reply_t          *keyboardMapping = xcb_get_keyboard_mapping_reply(
            _connection,
            xcb_get_keyboard_mapping(_connection, setup->min_keycode,
                                              setup->max_keycode - setup->min_keycode + 1),
            NULL);

        int           nkeycodes = keyboardMapping->length / keyboardMapping->keysyms_per_keycode;
        int           nkeysyms  = keyboardMapping->length;
        xcb_keysym_t *keysyms =
            (xcb_keysym_t *)(keyboardMapping + 1); // `xcb_keycode_t` is just a `typedef u8`, and
                                                   // `xcb_keysym_t` is just a `typedef u32`
        printf("nkeycodes %u  nkeysyms %u  keysyms_per_keycode %u\n\n", nkeycodes, nkeysyms,
               keyboardMapping->keysyms_per_keycode);

        for (int keycodeIdx = 0; keycodeIdx < nkeycodes; ++keycodeIdx) {
            keysymsMap[setup->min_keycode + keycodeIdx] =
                keysyms[0 + (keycodeIdx * keyboardMapping->keysyms_per_keycode)];
            putchar('\n');
        }

        free(keyboardMapping);
        return keysymsMap;
    }

    auto XcbConnect::get_default_screen() -> xcb_screen_t *
    {
        return xcb_setup_roots_iterator(xcb_get_setup(_connection)).data;
    }

    auto XcbConnect::get_default_root_window() -> XcbWindow
    {
        return XcbWindow(this, get_default_screen()->root, false);
    }

    auto XcbConnect::send_key_press(xcb_keycode_t keycode) -> void
    {
        xcb_test_fake_input(_connection, XCB_KEY_PRESS, keycode, XCB_CURRENT_TIME, XCB_NONE, 0, 0,
                            0);
        flush();
    }

    auto XcbConnect::send_key_release(xcb_keycode_t keycode) -> void
    {
        xcb_test_fake_input(_connection, XCB_KEY_RELEASE, keycode, XCB_CURRENT_TIME, XCB_NONE, 0, 0,
                            0);
        flush();
    }

    auto XcbConnect::send_mouse_move(int16_t rootX, int16_t rootY) -> void
    {
        xcb_test_fake_input(_connection, XCB_MOTION_NOTIFY, 0, XCB_CURRENT_TIME, XCB_NONE, rootX,
                            rootY, 0);
        flush();
    }

    auto XcbConnect::send_mouse_button_press(xcb_button_t button) -> void
    {
        xcb_test_fake_input(_connection, XCB_BUTTON_PRESS, button, XCB_CURRENT_TIME, XCB_NONE, 0, 0,
                            0);
        flush();
    }

    auto XcbConnect::send_mouse_button_release(xcb_button_t button) -> void
    {
        xcb_test_fake_input(_connection, XCB_BUTTON_RELEASE, button, XCB_CURRENT_TIME, XCB_NONE, 0,
                            0, 0);
        flush();
    }

    auto XcbConnect::_init_io_descriptor() -> void
    {
        auto fd = xcb_get_file_descriptor(_connection);
        _fd     = _context->addDescriptor(fd, IoDescriptor::Unknown).value();
    }

    auto XcbConnect::_destroy_io_descriptor() -> void
    {
        if (_fd != nullptr) {
            _context->removeDescriptor(_fd);
            _fd = nullptr;
        }
    }

    auto XcbConnect::_poll_event() -> IoTask<void>
    {
        auto ret = co_await _context->poll(_fd, PollEvent::In);
        if (ret && ret.value() == PollEvent::In) {
            co_return {};
        }
        co_return Unexpected<Error>(ret.error_or(Error::Unknown));
    }

    auto XcbConnect::connect(const char *displayname) -> IoTask<int>
    {
        int screen  = 0;
        _connection = xcb_connect(displayname, &screen);
        _init_io_descriptor();
        auto ret = _poll_event();
        if (ret) {
            if (xcb_connection_has_error(_connection) != 0) {
                disconnect();
                co_return Unexpected<Error>(Error::ConnectionAborted);
            }
            _keySymbols =
                std::unique_ptr<xcb_key_symbols_t, std::function<void(xcb_key_symbols_t *)>>(
                    xcb_key_symbols_alloc(_connection),
                    [](xcb_key_symbols_t *ptr) { xcb_key_symbols_free(ptr); });
            co_return screen;
        }
        co_return Unexpected<Error>(Error::Unknown);
    }

    auto XcbConnect::disconnect() -> void
    {
        _destroy_io_descriptor();
        if (_connection != nullptr) {
            xcb_disconnect(_connection);
            _connection = nullptr;
        }
    }

    auto XcbConnect::event_loop() -> IoTask<void>
    {
        if (_connection == nullptr || _fd == nullptr) {
            co_return {};
        }
        std::unique_ptr<xcb_generic_event_t> event;
        while (true) {
            auto result = co_await _poll_event();
            if (!result) {
                SPDLOG_ERROR("poll_event failed, {}", result.error().message());
                break;
            }
            while (true) {
                event.reset(xcb_poll_for_event(_connection));
                if (event == nullptr) {
                    break;
                }
                if (event->response_type == 0) {
                    xcb_generic_error_t *err = (xcb_generic_error_t *)event.get();
                    SPDLOG_ERROR("Error: {}", err->error_code);
                    auto item = _sequeueMap.find(event->sequence);
                    if (item != _sequeueMap.end()) {
                        item->second(std::move(event));
                        continue;
                    }
                }
                switch (event->response_type & ~0x80) {
                case XCB_KEY_PRESS:
                case XCB_KEY_RELEASE:
                    if (_keyboardCallback) {
                        _keyboardCallback(event.get());
                    }
                    break;
                case XCB_BUTTON_PRESS:
                case XCB_BUTTON_RELEASE:
                case XCB_MOTION_NOTIFY:
                    if (_pointerCallback) {
                        _pointerCallback(event.get());
                    }
                    break;
                default:
                    SPDLOG_ERROR("Unknown event type: {}", event->response_type & ~0x80);
                    break;
                }
            }
        }
        co_return {};
    }

    auto XcbConnect::grab_pointer(XcbWindow                                 *window,
                                  std::function<void(xcb_generic_event_t *)> callback, bool owner)
        -> int
    {
        xcb_window_t root = XCB_WINDOW_NONE;
        if (window == nullptr) {
            root = get_default_root_window().native_handle();
        }
        else {
            root = window->native_handle();
        }

        auto cookie = xcb_grab_pointer(
            connection(), static_cast<uint8_t>(owner), root,
            XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
                XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_BUTTON_1_MOTION |
                XCB_EVENT_MASK_BUTTON_2_MOTION | XCB_EVENT_MASK_BUTTON_3_MOTION |
                XCB_EVENT_MASK_BUTTON_4_MOTION | XCB_EVENT_MASK_BUTTON_5_MOTION |
                XCB_EVENT_MASK_BUTTON_MOTION,
            XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, XCB_NONE, XCB_NONE, XCB_CURRENT_TIME);
        std::unique_ptr<xcb_grab_pointer_reply_t> reply(
            xcb_grab_pointer_reply(connection(), cookie, nullptr));
        if (reply == nullptr) {
            SPDLOG_ERROR("Error: failed to grab pointer\n");
            return -1;
        }
        if (reply->status != XCB_GRAB_STATUS_SUCCESS) {
            SPDLOG_ERROR("Error: failed to grab pointer, error: {}\n", reply->status);
            return reply->status;
        }
        _pointerCallback = callback;
        return 0;
    }

    auto XcbConnect::ungrab_pointer() -> int
    {
        auto cookie = xcb_ungrab_pointer_checked(connection(), XCB_CURRENT_TIME);
        std::unique_ptr<xcb_generic_error_t> err(xcb_request_check(connection(), cookie));
        if (err) {
            SPDLOG_ERROR("Failed to ungrab pointer, error {}\n", err->error_code);
            return err->error_code;
        }
        return 0;
    }

    auto XcbConnect::grab_keyboard(XcbWindow                                 *window,
                                   std::function<void(xcb_generic_event_t *)> callback, bool owner)
        -> int
    {
        xcb_window_t root = XCB_WINDOW_NONE;
        if (window != nullptr) {
            root = window->native_handle();
        }
        else {
            root = get_default_root_window().native_handle();
        }
        auto cookie = xcb_grab_keyboard(connection(), static_cast<uint8_t>(owner), root,
                                        XCB_CURRENT_TIME, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
        std::unique_ptr<xcb_grab_keyboard_reply_t> ret(
            xcb_grab_keyboard_reply(connection(), cookie, nullptr));
        if (ret == nullptr) {
            SPDLOG_ERROR("Failed to grab keyboard.\n");
            return -1;
        }
        if (ret->status != XCB_GRAB_STATUS_SUCCESS) {
            SPDLOG_ERROR("Failed to grab keyboard, status {}\n", ret->status);
            return ret->status;
        }
        _keyboardCallback = callback;
        return 0;
    }

    auto XcbConnect::ungrab_keyboard() -> int
    {
        auto cookie = xcb_ungrab_keyboard_checked(connection(), XCB_CURRENT_TIME);
        std::unique_ptr<xcb_generic_error_t> err(xcb_request_check(connection(), cookie));
        if (err) {
            SPDLOG_ERROR("Failed to ungrab keyboard, error {}\n", err->error_code);
            return err->error_code;
        }
        return 0;
    }

    xcb_atom_t XcbConnect::get_atom(const char *name)
    {
        xcb_intern_atom_cookie_t cookie = xcb_intern_atom(_connection, 0, strlen(name), name);
        std::unique_ptr<xcb_intern_atom_reply_t> reply(
            xcb_intern_atom_reply(_connection, cookie, NULL));
        if (!reply) {
            SPDLOG_ERROR("Failed to get atom: {}\n", name);
            return XCB_ATOM_NONE;
        }
        return reply->atom;
    }

    XcbWindow::XcbWindow(XcbConnect *conn)
    {
        _window = conn->generate_id();
        _conn   = conn;
        _create_window();
    }

    auto XcbWindow::_create_window() -> void
    {
        auto    *screen   = _conn->get_default_screen();
        uint32_t values[] = {XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE |
                             XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
                             XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_POINTER_MOTION_HINT |
                             XCB_EVENT_MASK_BUTTON_1_MOTION | XCB_EVENT_MASK_BUTTON_2_MOTION |
                             XCB_EVENT_MASK_BUTTON_3_MOTION | XCB_EVENT_MASK_BUTTON_4_MOTION |
                             XCB_EVENT_MASK_BUTTON_5_MOTION | XCB_EVENT_MASK_BUTTON_MOTION};
        xcb_create_window(_conn->connection(), XCB_COPY_FROM_PARENT, _window, screen->root, 0, 0, 1,
                          1, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual,
                          XCB_CW_EVENT_MASK, values);
    }

    XcbWindow::XcbWindow(XcbConnect *conn, xcb_window_t window, bool destroyAble)
    {
        _window      = window;
        _conn        = conn;
        _destroyAble = destroyAble;
    }

    XcbWindow::~XcbWindow()
    {
        destroy();
    }

    void XcbWindow::destroy()
    {
        if (_window != XCB_WINDOW_NONE && _destroyAble) {
            xcb_destroy_window(_conn->connection(), _window);
            _window = 0;
        }
        else if (_window == XCB_WINDOW_NONE) {
            SPDLOG_INFO("Window already destroyed\n");
        }
    }

    void XcbWindow::show()
    {
        if (_window != XCB_WINDOW_NONE) {
            xcb_map_window(_conn->connection(), _window);
            _conn->flush();
        }
        else {
            SPDLOG_ERROR("Window not created\n");
        }
    }

    void XcbWindow::hide()
    {
        if (_window != XCB_WINDOW_NONE) {
            xcb_unmap_window(_conn->connection(), _window);
        }
        else {
            SPDLOG_ERROR("Window not created\n");
        }
    }

    void XcbWindow::set_geometry(int posx, int posy, int width, int height)
    {
        if (_window != XCB_WINDOW_NONE) {
            int data[4] = {posx, posy, width, height};
            xcb_configure_window(_conn->connection(), _window,
                                 XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                                     XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                                 data);
        }
        else {
            SPDLOG_ERROR("Window not created\n");
        }
    }

    void XcbWindow::get_geometry(int &posx, int &posy, int &width, int &height)
    {
        if (_window != XCB_WINDOW_NONE) {
            xcb_get_geometry_reply_t *reply = xcb_get_geometry_reply(
                _conn->connection(), xcb_get_geometry(_conn->connection(), _window), nullptr);
            posx   = reply->x;
            posy   = reply->y;
            width  = reply->width;
            height = reply->height;
            free(reply);
        }
        else {
            SPDLOG_ERROR("Window not created\n");
        }
    }

    auto XcbWindow::set_property(const std::string &name, const std::string &value) -> int
    {
        auto atom = _conn->get_atom(name.c_str());
        if (atom == XCB_ATOM_NONE) {
            return -1;
        }
        auto cookie =
            xcb_change_property_checked(_conn->connection(), XCB_PROP_MODE_REPLACE, _window, atom,
                                        XCB_ATOM_STRING, 8, value.size(), value.c_str());
        std::unique_ptr<xcb_generic_error_t> reply(xcb_request_check(_conn->connection(), cookie));
        if (reply) {
            SPDLOG_ERROR("Failed to set property {}, error: {}\n", name.c_str(),
                          reply->error_code);
            return -1;
        }
        return 0;
    }

    auto XcbWindow::set_attribute(uint32_t eventMask, uint32_t values[]) -> int
    {
        auto cookie =
            xcb_change_window_attributes_checked(_conn->connection(), _window, eventMask, values);
        std::unique_ptr<xcb_generic_error_t> reply(xcb_request_check(_conn->connection(), cookie));
        if (reply) {
            SPDLOG_ERROR("Failed to set attribute, error: {}\n", reply->error_code);
            return -1;
        }
        return 0;
    }

    auto XcbWindow::set_transparent(uint32_t alpha) -> void
    {
        xcb_change_property(_conn->connection(), XCB_PROP_MODE_REPLACE, _window, XCB_ATOM_CARDINAL,
                            XCB_ATOM_CARDINAL, 32, 1, &alpha);
    }

    auto XcbWindow::native_handle() const -> xcb_window_t
    {
        return _window;
    }

} // namespace mks::base
#endif