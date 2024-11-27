#include "xcbcapture.hpp"
#include <xcb/xtest.h>

XCB_NAMESPACE_BEGIN

int XcbConnect::generate_id()
{
    return xcb_generate_id(_connection);
}

auto XcbConnect::get_default_screen() -> xcb_screen_t *
{
    return xcb_setup_roots_iterator(xcb_get_setup(_connection)).data;
}

auto XcbConnect::get_default_root_window() -> XcbWindow
{
    return XcbWindow(this, get_default_screen()->root, false);
}

auto XcbConnect::send_key_press(xcb_keycode_t keycode) -> Task<void>
{
    //--------------------------
    // XTestFakeKeyEvent(_display, keycode, 1, CurrentTime);
    // XFlush(_display);
    // co_return {};
    //--------------------------
    xcb_test_fake_input(_connection, XCB_KEY_PRESS, keycode, XCB_CURRENT_TIME, XCB_NONE, 0, 0, 0);
    flush();
    co_return {};
}

auto XcbConnect::send_key_release(xcb_keycode_t keycode) -> Task<void>
{
    //--------------------------
    // XTestFakeKeyEvent(_display, keycode, 0, CurrentTime);
    // XFlush(_display);
    // co_return {};
    //--------------------------
    xcb_test_fake_input(_connection, XCB_KEY_RELEASE, keycode, XCB_CURRENT_TIME, XCB_NONE, 0, 0, 0);
    flush();
    co_return {};
}

auto XcbConnect::send_mouse_move(int16_t rootX, int16_t rootY) -> Task<void>
{
    xcb_test_fake_input(_connection, XCB_MOTION_NOTIFY, 0, XCB_CURRENT_TIME, XCB_NONE, 0, rootX, rootY);
    flush();
    return {};
}

auto XcbConnect::send_mouse_button_press(xcb_button_t button) -> Task<void>
{
    xcb_test_fake_input(_connection, XCB_BUTTON_PRESS, button, XCB_CURRENT_TIME, XCB_NONE, 0, 0, 0);
    flush();
    return {};
}

auto XcbConnect::send_mouse_button_release(xcb_button_t button) -> Task<void>
{
    xcb_test_fake_input(_connection, XCB_BUTTON_RELEASE, button, XCB_CURRENT_TIME, XCB_NONE, 0, 0, 0);
    flush();
    return {};
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

auto XcbConnect::_poll_event() -> Task<void>
{
    auto ret = co_await _context->poll(_fd, PollEvent::In);
    if (ret && ret.value() == PollEvent::In) {
        co_return {};
    }
    co_return Unexpected<Error>(ret.error_or(Error::Unknown));
}

auto XcbConnect::connect(const char *displayname, int *screenp) -> Task<void>
{
    _connection = xcb_connect(displayname, screenp);
    _display    = XOpenDisplay(displayname);
    _init_io_descriptor();
    auto ret = _poll_event();
    if (ret) {
        if (xcb_connection_has_error(_connection) != 0) {
            disconnect();
            co_return Unexpected<Error>(Error::ConnectionAborted);
        }
        _keySymbols = std::unique_ptr<xcb_key_symbols_t, std::function<void(xcb_key_symbols_t *)>>(
            xcb_key_symbols_alloc(_connection),
            [](xcb_key_symbols_t *ptr) { xcb_key_symbols_free(ptr); });
        co_return {};
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

auto XcbConnect::event_loop() -> Task<void>
{
    if (_connection == nullptr || _fd == nullptr) {
        std::cerr << "connection is null" << std::endl;
        co_return {};
    }
    std::unique_ptr<xcb_generic_event_t> event;

    while (true) {
        auto result = co_await _poll_event();

        if (!result) {
            std::cout << "Error: " << result.error().message() << std::endl;
            break;
        }
        while (true) {
            event.reset(xcb_poll_for_event(_connection));
            if (event == nullptr) {
                break;
            }

            if ((event->response_type & ~0x80) != 0) {
                if (event->response_type == 0) {
                    xcb_generic_error_t *err = (xcb_generic_error_t *)event.get();
                    std::cerr << "Error: " << err->error_code << std::endl;
                    auto item = _sequeueMap.find(event->sequence);
                    if (item != _sequeueMap.end()) {
                        item->second(std::move(event));
                        continue;
                    }
                }
                else {
                    auto ret = co_await event_dispatcher(std::move(event));
                    if (!ret) {
                        std::cout << "Error: " << ret.error().message() << std::endl;
                        co_return Unexpected<Error>(ret.error());
                    }
                }
            }
        }
    }
    co_return Unexpected<Error>(Error::Unknown);
}

auto XcbConnect::grab_pointer(XcbWindow *window, bool owner) -> int
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
        printf("Error: failed to grab pointer\n");
        return -1;
    }
    if (reply->status != XCB_GRAB_STATUS_SUCCESS) {
        printf("Error: failed to grab pointer, error: %d\n", reply->status);
        return reply->status;
    }
    return 0;
}

auto XcbConnect::ungrab_pointer() -> int
{
    auto cookie = xcb_ungrab_pointer_checked(connection(), XCB_CURRENT_TIME);
    std::unique_ptr<xcb_generic_error_t> err(xcb_request_check(connection(), cookie));
    if (err) {
        printf("Failed to ungrab pointer, error %d\n", err->error_code);
        return err->error_code;
    }
    return 0;
}

auto XcbConnect::grab_keyboard(XcbWindow *window, bool owner) -> int
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
        printf("Failed to grab keyboard.\n");
        return -1;
    }
    if (ret->status != XCB_GRAB_STATUS_SUCCESS) {
        printf("Failed to grab keyboard, status %d\n", ret->status);
        return ret->status;
    }
    return 0;
}

auto XcbConnect::ungrab_keyboard() -> int
{
    auto cookie = xcb_ungrab_keyboard_checked(connection(), XCB_CURRENT_TIME);
    std::unique_ptr<xcb_generic_error_t> err(xcb_request_check(connection(), cookie));
    if (err) {
        printf("Failed to ungrab keyboard, error %d\n", err->error_code);
        return err->error_code;
    }
    return 0;
}

auto XcbConnect::event_dispatcher(std::unique_ptr<xcb_generic_event_t> event) -> Task<void>
{
    auto *keysyms = key_symbols();
    switch (event->response_type & ~0x80) {
    case XCB_KEY_PRESS: {
        xcb_key_press_event_t *keyEvent = (xcb_key_press_event_t *)event.get();
        // 获取按键的符号
        xcb_keysym_t keysym = xcb_key_symbols_get_keysym(keysyms, keyEvent->detail, 0);

        // 将符号转换为字符
        char character = '?';
        if (keysym >= 0x20 && keysym <= 0x7E) { // 仅处理可打印字符
            character = keysym;
        }
        std::cout << "Key pressed: " << (int)keyEvent->detail << " value: " << character
                  << std::endl;
        if (character == 'q' || character == 'Q' || keyEvent->detail == 0x0009) {
            co_return Unexpected<Error>(Error::Unknown);
        }
        break;
    }
    case XCB_KEY_RELEASE: {
        xcb_key_release_event_t *keyEvent = (xcb_key_release_event_t *)event.get();
        std::cout << "Key released: " << (int)keyEvent->detail << std::endl;
        break;
    }
    case XCB_MOTION_NOTIFY: {
        xcb_motion_notify_event_t *motionEvent = (xcb_motion_notify_event_t *)event.get();
        std::cout << "Mouse moved to (" << (int)motionEvent->root_x << ", "
                  << (int)motionEvent->root_y << ")" << std::endl;
        break;
    }
    case XCB_BUTTON_PRESS: {
        xcb_button_press_event_t *buttonEvent = (xcb_button_press_event_t *)event.get();
        // 判断鼠标按钮
        switch (buttonEvent->detail) {
        case 1:
            printf("Left mouse button pressed.\n");
            break;
        case 2:
            printf("Middle mouse button pressed.\n");
            break;
        case 3:
            printf("Right mouse button pressed.\n");
            break;
        default:
            printf("Other button pressed: %u\n", buttonEvent->detail);
            break;
        }
        break;
    }
    case XCB_BUTTON_RELEASE: {
        xcb_button_release_event_t *buttonEvent = (xcb_button_release_event_t *)event.get();
        // 判断鼠标按钮
        switch (buttonEvent->detail) {
        case 1:
            printf("Left mouse button released.\n");
            break;
        case 2:
            printf("Middle mouse button released.\n");
            break;
        case 3:
            printf("Right mouse button released.\n");
            break;
        default:
            printf("Other button released: %u\n", buttonEvent->detail);
            break;
        }
        break;
    }
    default:
        printf("Unknown event type: %u\n", event->response_type & ~0x80);
        break;
    }
    for (auto *item : _windows) {
        auto ret = co_await item->event_loop(std::move(event));
    }
    co_return {};
}

xcb_atom_t XcbConnect::get_atom(const char *name)
{
    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(_connection, 0, strlen(name), name);
    std::unique_ptr<xcb_intern_atom_reply_t> reply(
        xcb_intern_atom_reply(_connection, cookie, NULL));
    if (!reply) {
        printf("Failed to get atom: %s\n", name);
        return XCB_ATOM_NONE;
    }
    return reply->atom;
}

XcbWindow::XcbWindow(XcbConnect *conn)
{
    conn->add_root_window(this);
    _window = conn->generate_id();
    _conn   = conn;
    _create_window();
}

XcbWindow::XcbWindow(XcbWindow *parent)
{
    _conn   = parent->_conn;
    _window = _conn->generate_id();
    parent->_children.insert(this);
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
    xcb_create_window(_conn->connection(), XCB_COPY_FROM_PARENT, _window, screen->root, 0, 0, 1, 1,
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, XCB_CW_EVENT_MASK,
                      values);
}

XcbWindow::XcbWindow(XcbConnect *conn, xcb_window_t window, bool destroyAble)
{
    _window      = window;
    _conn        = conn;
    _destroyAble = destroyAble;
    conn->add_root_window(this);
}

XcbWindow::~XcbWindow()
{
    destroy();
}

void XcbWindow::destroy()
{
    if (_window != XCB_WINDOW_NONE && _destroyAble) {
        for (const auto &child : _children) {
            child->destroy();
        }
        xcb_destroy_window(_conn->connection(), _window);
        _window = 0;
    }
    else if (_window == XCB_WINDOW_NONE) {
        printf("Window already destroyed\n");
    }
    _conn->remove_root_window(this);
}

int XcbWindow::grab_keyboard(bool grab, bool ownerEvents)
{
    if (_grabbedKeyboard == grab) {
        return 0;
    }
    if (grab) {
        auto ret = _conn->grab_keyboard(this, ownerEvents);
        if (ret == 0) {
            _grabbedKeyboard = true;
        }
        return ret;
    }
    auto ret = _conn->ungrab_keyboard();
    if (ret == 0) {
        _grabbedKeyboard = false;
    }
    return ret;
}

int XcbWindow::grab_pointer(bool grab, bool ownerEvents)
{
    if (_grabbedPointer == grab) {
        return 0;
    }
    if (grab) {
        auto ret = _conn->grab_pointer(this, ownerEvents);
        if (ret == 0) {
            _grabbedPointer = true;
        }
        return ret;
    }

    auto ret = _conn->ungrab_pointer();
    if (ret == 0) {
        _grabbedPointer = false;
    }
    return ret;
}

void XcbWindow::show()
{
    if (_window != XCB_WINDOW_NONE) {
        xcb_map_window(_conn->connection(), _window);
        _conn->flush();
    }
    else {
        printf("Window not created\n");
    }
}

void XcbWindow::hide()
{
    if (_window != XCB_WINDOW_NONE) {
        xcb_unmap_window(_conn->connection(), _window);
    }
    else {
        printf("Window not created\n");
    }
}

void XcbWindow::set_geometry(int posx, int posy, int width, int height)
{
    if (_window != XCB_WINDOW_NONE) {
        int data[4] = {posx, posy, width, height};
        xcb_configure_window(_conn->connection(), _window,
                             XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH |
                                 XCB_CONFIG_WINDOW_HEIGHT,
                             data);
    }
    else {
        printf("Window not created\n");
    }
}

auto XcbWindow::event_loop(std::unique_ptr<xcb_generic_event_t> event) -> Task<void>
{
    auto *keysyms = _conn->key_symbols();
    switch (event->response_type & ~0x80) {
    case XCB_KEY_PRESS: {
        xcb_key_press_event_t *keyEvent = (xcb_key_press_event_t *)event.get();
        // 获取按键的符号
        xcb_keysym_t keysym = xcb_key_symbols_get_keysym(keysyms, keyEvent->detail, 0);

        // 将符号转换为字符
        char character = '?';
        if (keysym >= 0x20 && keysym <= 0x7E) { // 仅处理可打印字符
            character = keysym;
        }
        if (character == 'k' || character == 'k') {
            printf("grab keyboard\n");
            usleep(1000000); // 1s
            grab_keyboard(true);
        }
        else if (character == 'u' || character == 'U') {
            printf("ungrab all\n");
            grab_keyboard(false);
            grab_pointer(false);
        }
        else if (character == 'p' || character == 'P') {
            printf("grab pointer\n");
            usleep(1000000);
            grab_pointer(true);
        }
        else if (character == 'w' || character == 'W') {
            char wf[] = "this is a test to send keyboard";
            printf("send keyboard\n");
            for (int i = 0; i < (int)sizeof(wf); i++) {
                auto keycode = xcb_key_symbols_get_keycode(keysyms, wf[i])[0];
                co_await _conn->send_key_press(keycode);
                usleep(100000);
                co_await _conn->send_key_release(keycode);
            }
        }
        break;
    }
    default:
        break;
    }
    co_return {};
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
        printf("Failed to set property %s, error: %d\n", name.c_str(), reply->error_code);
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
        printf("Failed to set attribute, error: %d\n", reply->error_code);
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

XCB_NAMESPACE_END