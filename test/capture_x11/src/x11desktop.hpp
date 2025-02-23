#pragma once
#include "xcbcapture.hpp"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <cstdint>
#include <unordered_map>
#include <iostream>
#include <thread>
#include <chrono>

#define TEMP_NAMESPACE x11dspace
#define TEMP_NAMESPACE_BEGIN                                                                       \
    namespace TEMP_NAMESPACE                                                                       \
    {
#define TEMP_NAMESPACE_END }

TEMP_NAMESPACE_BEGIN
class X11Window;

class X11Desktop {
public:
    X11Desktop() {}
    X11Desktop(const X11Desktop &)            = delete;
    X11Desktop &operator=(const X11Desktop &) = delete;
    X11Desktop(X11Desktop &&)                 = delete;
    X11Desktop &operator=(X11Desktop &&)      = delete;
    ~X11Desktop();
    auto init(const char *displayName = nullptr) -> int
    {
        Display *display = XOpenDisplay(displayName);
        if (display == nullptr) {
            return -1;
        }
        this->_display = display;
        return 0;
    }
    auto uninit() -> void;
    auto get_display() -> Display * { return _display; }
    auto get_defaule_screen_number() -> int { return DefaultScreen(_display); }
    auto get_defaule_screen() -> Screen * { return XDefaultScreenOfDisplay(_display); }
    auto get_default_root_window() -> Window { return DefaultRootWindow(_display); }
    auto get_root_window(int screen) -> Window { return RootWindow(_display, screen); }
    auto grab_mouse(Window window) -> int
    {
        return XGrabPointer(_display, window, True,
                            PointerMotionMask | ButtonPressMask | ButtonReleaseMask, GrabModeAsync,
                            GrabModeAsync, None, None, CurrentTime);
    }
    auto release_mouse() -> int { return XUngrabPointer(_display, CurrentTime); }
    auto grab_keyboard(Window window) -> int
    {
        return XGrabKeyboard(_display, window, True, GrabModeAsync, GrabModeAsync, CurrentTime);
    }
    auto release_keyboard() -> int { return XUngrabKeyboard(_display, CurrentTime); }
    auto intern_atom(const char *name, int onlyIfExists) -> Atom
    {
        return XInternAtom(_display, name, onlyIfExists);
    }
    auto get_next_event(XEvent &event) -> int { return XNextEvent(_display, &event); }

    static auto get_root_window(Screen *screen) -> Window { return RootWindowOfScreen(screen); }

private:
    Display *_display;
};

class X11Window {
public:
    X11Window(X11Desktop &desktop, X11Window *parent = nullptr) : _desktop(desktop), _parent(parent)
    {
        if (parent != nullptr) {
            _parent->add_child(this);
        }
    }
    X11Window(X11Window &&other)                 = delete;
    X11Window &operator=(X11Window &&other)      = delete;
    X11Window(const X11Window &other)            = delete;
    X11Window &operator=(const X11Window &other) = delete;
    ~X11Window() { uninit(); }

    auto get_window() const -> Window { return _window; }
    auto init(int sx = 0, int sy = 0, int width = 800, int height = 600) -> void
    {
        _window = XCreateSimpleWindow(
            _desktop.get_display(),
            _parent != nullptr ? _parent->get_window()
                               : _desktop.get_root_window(_desktop.get_defaule_screen_number()),
            sx, sy, width, height, 1,
            BlackPixel(_desktop.get_display(), _desktop.get_defaule_screen_number()),
            WhitePixel(_desktop.get_display(), _desktop.get_defaule_screen_number()));
    }
    auto uninit() -> void
    {
        for (auto &child : _children) {
            child.second->uninit();
        }
        if (_window != None) {
            XDestroyWindow(_desktop.get_display(), _window);
            _window = None;
        }
    }
    auto add_child(X11Window *child) -> void
    {
        _children.insert(std::make_pair(child->_window, child));
    }
    auto get_desktop() -> X11Desktop * { return &_desktop; }
    auto get_parent() -> X11Window * { return _parent; }
    auto get_root_window() -> Window
    {
        if (_parent != nullptr) {
            return _parent->get_root_window();
        }
        return _desktop.get_root_window(_desktop.get_defaule_screen_number());
    }
    auto set_property(Atom property, Atom type, int format, int mode, const unsigned char *data,
                      int nelements) -> int
    {
        return XChangeProperty(_desktop.get_display(), _window, property, type, format, mode, data,
                               nelements);
    }
    auto map_window() -> void { XMapWindow(_desktop.get_display(), _window); }
    auto unmap_window() -> void { XUnmapWindow(_desktop.get_display(), _window); }
    auto show() -> void { XMapRaised(_desktop.get_display(), _window); }
    auto set_window_title(const char *title) -> void
    {
        XStoreName(_desktop.get_display(), _window, title);
    }
    auto grab_keyboard()
    {
        return XGrabKeyboard(_desktop.get_display(), _window, False, GrabModeAsync, GrabModeAsync,
                             CurrentTime);
    }
    auto release_keyboard() { return XUngrabKeyboard(_desktop.get_display(), CurrentTime); }
    auto grab_mouse()
    {
        return XGrabPointer(_desktop.get_display(), _window, False,
                            ButtonPressMask | ButtonReleaseMask | PointerMotionMask, GrabModeAsync,
                            GrabModeAsync, _window, None, CurrentTime);
    }
    auto release_mouse() { return XUngrabPointer(_desktop.get_display(), CurrentTime); }

private:
    X11Desktop                               &_desktop;
    X11Window                                *_parent;
    Window                                    _window;
    std::unordered_map<uint64_t, X11Window *> _children;
};

inline X11Desktop::~X11Desktop()
{
    uninit();
}

inline auto X11Desktop::uninit() -> void
{
    if (_display != nullptr) {
        XCloseDisplay(_display);
    }
}

int x11Example()
{
    // 打开 X 显示
    TEMP_NAMESPACE::X11Desktop desktop;
    if (desktop.init() != 0) {
        std::cerr << "Unable to open X display." << std::endl;
        return -1;
    }

    // 创建一个窗口
    TEMP_NAMESPACE::X11Window window(desktop);
    window.init();

    // 设置窗口属性
    Atom wmType       = desktop.intern_atom("_NET_WM_WINDOW_TYPE", False);
    Atom wmTypeDialog = desktop.intern_atom("_NET_WM_WINDOW_TYPE_DIALOG", False);
    window.set_property(wmType, XA_ATOM, 32, PropModeReplace, (unsigned char *)&wmTypeDialog, 1);
    Atom          wmOpacity = desktop.intern_atom("_NET_WM_WINDOW_OPACITY", False);
    unsigned long opacity   = 0xffffffff;
    window.set_property(wmOpacity, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&opacity, 1);

    window.map_window();

    bool mGrabbed = false;
    bool kGrabbed = false;

    // 事件循环
    XEvent event;
    while (true) {
        // 抓取鼠标
        if (!mGrabbed) {
            auto ret = window.grab_mouse();
            if (ret != GrabSuccess) {
                std::cerr << "Unable to grab pointer. error code: " << ret << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            mGrabbed = true;
        }

        // 抓取键盘
        if (!kGrabbed) {
            auto ret2 = window.grab_keyboard();
            if (ret2 != GrabSuccess) {
                std::cerr << "Unable to grab keyboard. error code: " << ret2 << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            mGrabbed = true;
        }

        desktop.get_next_event(event);

        // 处理鼠标事件
        if (event.type == MotionNotify) {
            std::cout << "Mouse moved to (" << event.xmotion.x << ", " << event.xmotion.y << ")."
                      << std::endl;
        }
        else if (event.type == ButtonPress) {
            KeyCode key = event.xbutton.button;
            if (key == Button1) {
                std::cout << "Left mouse button pressed." << std::endl;
            }
            else if (key == Button2) {
                std::cout << "Middle mouse button pressed." << std::endl;
            }
            else if (key == Button3) {
                std::cout << "Right mouse button pressed." << std::endl;
            }
        }
        else if (event.type == ButtonRelease) {
            KeyCode key = event.xbutton.button;
            if (key == Button1) {
                std::cout << "Left mouse button released." << std::endl;
            }
            else if (key == Button2) {
                std::cout << "Middle mouse button released." << std::endl;
            }
            else if (key == Button3) {
                std::cout << "Right mouse button released." << std::endl;
            }
        }

        // 处理键盘事件
        if (event.type == KeyPress) {
            char   buffer[1];
            KeySym keysym;
            XLookupString(&event.xkey, buffer, sizeof(buffer), &keysym, nullptr);
            std::cout << "Key pressed - " << " code: " << keysym << " value: " << buffer[0]
                      << std::endl;
            if (keysym == XK_Escape) {
                break;
            }
        }
        else if (event.type == KeyRelease) {
            char   buffer[1];
            KeySym keysym;
            XLookupString(&event.xkey, buffer, sizeof(buffer), &keysym, nullptr);
            std::cout << "Key released -"
                         " code: "
                      << keysym << std::endl;
        }

        std::cout << "Event type: " << event.type << std::endl;
    }

    // 释放抓取
    desktop.release_keyboard();
    desktop.release_mouse();
    return 0;
}

TEMP_NAMESPACE_END