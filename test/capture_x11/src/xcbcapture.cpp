#include "xcbcapture.hpp"

XCB_NAMESPACE_BEGIN

int XcbConnect::generate_id()
{
    return xcb_generate_id(_connection);
}

auto XcbConnect::get_default_screen() -> xcb_screen_t *
{
    return xcb_setup_roots_iterator(xcb_get_setup(_connection)).data;
}

auto XcbConnect::get_default_root_window() -> xcb_window_t
{
    return get_default_screen()->root;
}

auto XcbConnect::send_key_press(xcb_keycode_t keycode) -> Task<void>
{
    auto window = get_default_root_window();
    // 发送按下事件
    xcb_key_press_event_t event  = xcb_key_press_event_t{.response_type = XCB_KEY_PRESS,
                                                         .detail        = keycode,
                                                         .sequence      = 0,
                                                         .time          = XCB_CURRENT_TIME,
                                                         .root          = XCB_NONE,
                                                         .event         = window,
                                                         .child         = XCB_NONE,
                                                         .root_x        = 0,
                                                         .root_y        = 0,
                                                         .event_x       = 0,
                                                         .event_y       = 0,
                                                         .state         = 0,
                                                         .same_screen   = 1,
                                                         .pad0          = 0};
    auto                  cookie = xcb_send_event(_connection, 0, window, XCB_EVENT_MASK_KEY_PRESS,
                                                  reinterpret_cast<const char *>(&event));
    (void)cookie;
    // _sequeueMap.emplace(cookie.sequence, [](std::unique_ptr<xcb_generic_event_t> &&event) {
    //     xcb_generic_error_t *error = (xcb_generic_error_t *)event.get();
    //     std::cout << "key press, error: " << error->error_code << std::endl;
    // });
    flush();
    co_return {};
}

auto XcbConnect::send_key_release(xcb_keycode_t keycode) -> Task<void>
{
    auto window = get_default_root_window();

    xcb_key_release_event_t releaseEvent = {.response_type = XCB_KEY_RELEASE,
                                            .detail        = keycode,
                                            .sequence      = 0,
                                            .time          = XCB_CURRENT_TIME,
                                            .root          = XCB_NONE,
                                            .event         = window,
                                            .child         = XCB_NONE,
                                            .root_x        = 0,
                                            .root_y        = 0,
                                            .event_x       = 0,
                                            .event_y       = 0,
                                            .state         = 0,
                                            .same_screen   = 1,
                                            .pad0          = 0};
    auto cookie = xcb_send_event(_connection, 0, window, XCB_EVENT_MASK_KEY_RELEASE,
                                 reinterpret_cast<const char *>(&releaseEvent));
    (void)cookie;
    // _sequeueMap.emplace(cookie.sequence, [](std::unique_ptr<xcb_generic_event_t> &&event) {
    //     xcb_generic_error_t *ev = (xcb_generic_error_t *)event.get();
    //     std::cout << "Key release, error: " << ev->error_code << std::endl;
    // });
    flush();
    co_return {};
}

auto XcbConnect::send_mouse_move(int16_t rootX, int16_t rootY) -> Task<void>
{
    auto                      window      = get_default_root_window();
    xcb_motion_notify_event_t motionEvent = {.response_type = XCB_MOTION_NOTIFY,
                                             .detail        = 0,
                                             .sequence      = 0,
                                             .time          = XCB_CURRENT_TIME,
                                             .root          = XCB_NONE,
                                             .event         = window,
                                             .child         = XCB_NONE,
                                             .root_x        = rootX,
                                             .root_y        = rootY,
                                             .event_x       = 0,
                                             .event_y       = 0,
                                             .state         = 0,
                                             .same_screen   = 1,
                                             .pad0          = 0};
    auto coockie = xcb_send_event(_connection, 0, window, XCB_EVENT_MASK_POINTER_MOTION,
                                  reinterpret_cast<const char *>(&motionEvent));
    (void)coockie;
    flush();
    return {};
}

auto XcbConnect::send_mouse_button_press(xcb_button_t button) -> Task<void>
{
    auto                     window     = get_default_root_window();
    xcb_button_press_event_t pressEvent = {.response_type = XCB_BUTTON_PRESS,
                                           .detail        = button,
                                           .sequence      = 0,
                                           .time          = 0,
                                           .root          = XCB_NONE,
                                           .event         = window,
                                           .child         = XCB_NONE,
                                           .root_x        = 0,
                                           .root_y        = 0,
                                           .event_x       = 0,
                                           .event_y       = 0,
                                           .state         = 0,
                                           .same_screen   = 1,
                                           .pad0          = 0};
    auto coockie = xcb_send_event(_connection, 0, window, XCB_EVENT_MASK_BUTTON_PRESS,
                                  reinterpret_cast<const char *>(&pressEvent));
    (void)coockie;
    flush();
    return {};
}

auto XcbConnect::send_mouse_button_release(xcb_button_t button) -> Task<void>
{
    auto window = get_default_root_window();
    // 发送鼠标释放事件
    xcb_button_release_event_t releaseEvent = {.response_type = XCB_BUTTON_RELEASE,
                                               .detail        = button,
                                               .sequence      = 0,
                                               .time          = 0,
                                               .root          = XCB_NONE,
                                               .event         = window,
                                               .child         = XCB_NONE,
                                               .root_x        = 0,
                                               .root_y        = 0,
                                               .event_x       = 0,
                                               .event_y       = 0,
                                               .state         = 0,
                                               .same_screen   = 1,
                                               .pad0          = 0};
    auto coockie = xcb_send_event(_connection, 0, window, XCB_EVENT_MASK_BUTTON_RELEASE,
                                  reinterpret_cast<const char *>(&releaseEvent));
    (void)coockie;
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
        event.reset(xcb_wait_for_event(_connection));
        if (event == nullptr) {
            continue;
        }

        auto item = _sequeueMap.find(event->sequence);
        if (item != _sequeueMap.end()) {
            item->second(std::move(event));
            continue;
        }
        if ((event->response_type & ~0x80) != 0) {
            if (event->response_type == 0) {
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
    co_return Unexpected<Error>(Error::Unknown);
}

auto XcbConnect::event_dispatcher(std::unique_ptr<xcb_generic_event_t> &&event) -> Task<void>
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
        break;
    }
    for (auto *item : _windows) {
        auto ret = co_await item->event_loop(std::move(event));
    }
    co_return {};
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
    uint32_t values[] = {screen->black_pixel,
                         XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE |
                             XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
                             XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW |
                             XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_POINTER_MOTION_HINT |
                             XCB_EVENT_MASK_BUTTON_1_MOTION | XCB_EVENT_MASK_BUTTON_2_MOTION |
                             XCB_EVENT_MASK_BUTTON_3_MOTION | XCB_EVENT_MASK_BUTTON_4_MOTION |
                             XCB_EVENT_MASK_BUTTON_5_MOTION | XCB_EVENT_MASK_BUTTON_MOTION |
                             XCB_EVENT_MASK_KEYMAP_STATE | XCB_EVENT_MASK_EXPOSURE |
                             XCB_EVENT_MASK_VISIBILITY_CHANGE | XCB_EVENT_MASK_STRUCTURE_NOTIFY |
                             XCB_EVENT_MASK_RESIZE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
                             XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_FOCUS_CHANGE |
                             XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_COLOR_MAP_CHANGE};
    xcb_create_window(_conn->connection(), XCB_COPY_FROM_PARENT, _window, screen->root, 0, 0, 800,
                      600, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual,
                      XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK, values);
}

XcbWindow::XcbWindow(XcbConnect *conn, xcb_window_t window)
{
    _window = window;
    _conn   = conn;
    conn->add_root_window(this);
}

XcbWindow::~XcbWindow()
{
    destroy();
}

void XcbWindow::destroy()
{
    if (_window != XCB_WINDOW_NONE) {
        for (const auto &child : _children) {
            child->destroy();
        }
        xcb_destroy_window(_conn->connection(), _window);
        _window = 0;
    }
    else {
        printf("Window already destroyed\n");
    }
}

void XcbWindow::grab_keyboard(bool grab, bool ownerEvents)
{
    if (grab) {
        xcb_grab_keyboard_unchecked(_conn->connection(), static_cast<uint8_t>(ownerEvents), _window,
                                    XCB_CURRENT_TIME, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    }
    else {
        xcb_ungrab_keyboard(_conn->connection(), XCB_CURRENT_TIME);
    }
}

void XcbWindow::grab_pointer(bool grab, bool ownerEvents)
{
    if (grab) {
        xcb_grab_pointer_unchecked(_conn->connection(), static_cast<uint8_t>(ownerEvents), _window,
                                   XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
                                       XCB_EVENT_MASK_POINTER_MOTION,
                                   XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, XCB_NONE, 0, 0);
    }
    else {
        xcb_ungrab_pointer(_conn->connection(), XCB_CURRENT_TIME);
    }
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

auto XcbWindow::event_loop(std::unique_ptr<xcb_generic_event_t> &&event) -> Task<void>
{
    auto *keysyms = _conn->key_symbols();
    switch (event->response_type & 0x80) {
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
            printf("grab keyboard");
            grab_keyboard(true, true);
        }
        else if (character == 'u' || character == 'U') {
            printf("ungrab all");
            grab_keyboard(false, true);
            grab_pointer(false, true);
        }
        else if (character == 'p' || character == 'P') {
            printf("grab pointer");
            grab_pointer(true, true);
        }
        break;
    }
    }
    co_return {};
}

XCB_NAMESPACE_END