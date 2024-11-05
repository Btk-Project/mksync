#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <iostream>
#include <thread>

void clipboardCallback(XPointer call_data)
{
    // 处理剪切板内容
    XSelectionEvent *sev = (XSelectionEvent *)call_data;
    if (sev->property != None) {
        Atom           actualType;
        int            actualFormat;
        unsigned long  nItems, bytesAfter;
        unsigned char *data = nullptr;

        // 获取剪切板内容
        if (XGetWindowProperty(sev->display, sev->requestor, sev->property, 0, 1024, False,
                               AnyPropertyType, &actualType, &actualFormat, &nItems, &bytesAfter,
                               &data) == Success) {
            if (data != nullptr) {
                std::cout << "Clipboard content: " << data << std::endl;
                XFree(data);
            }
        }
    }
}

int main()
{
    // 打开 X 显示
    Display *display = XOpenDisplay(nullptr);
    if (display == nullptr) {
        std::cerr << "Unable to open X display." << std::endl;
        return -1;
    }

    // 获取默认屏幕
    int    screen     = DefaultScreen(display);
    Window rootWindow = RootWindow(display, screen);

    // 获取剪切板拥有者
    Window clipboardOwner = XGetSelectionOwner(display, XA_PRIMARY);
    if (clipboardOwner == None) {
        std::cerr << "No owner for the clipboard." << std::endl;
        XCloseDisplay(display);
        return -1;
    }

    // 请求剪切板内容
    XConvertSelection(display, XA_PRIMARY, XA_STRING, XA_PRIMARY, rootWindow, CurrentTime);

    // 创建一个窗口
    Window window = XCreateSimpleWindow(display, rootWindow, 0, 0, 900, 600, 1,
                                        BlackPixel(display, screen), WhitePixel(display, screen));

    // 设置窗口属性
    Atom wmType       = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
    Atom wmTypeDialog = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    XChangeProperty(display, window, wmType, XA_ATOM, 32, PropModeReplace,
                    (unsigned char *)&wmTypeDialog, 1);
    Atom          wmOpacity = XInternAtom(display, "_NET_WM_WINDOW_OPACITY", False);
    unsigned long opacity   = 0xffffffff;
    XChangeProperty(display, window, wmOpacity, XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char *)&opacity, 1);

    XMapWindow(display, window);

    bool mGrabbed = false;
    bool kGrabbed = false;

    // 事件循环
    XEvent event;
    while (true) {
        // 抓取鼠标
        if (!mGrabbed) {
            auto ret = XGrabPointer(display, window, True,
                                    PointerMotionMask | ButtonPressMask | ButtonReleaseMask,
                                    GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
            if (ret != GrabSuccess) {
                std::cerr << "Unable to grab pointer. error code: " << ret << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            mGrabbed = true;
        }

        // 抓取键盘
        if (!kGrabbed) {
            auto ret2 =
                XGrabKeyboard(display, window, True, GrabModeAsync, GrabModeAsync, CurrentTime);
            if (ret2 != GrabSuccess) {
                std::cerr << "Unable to grab keyboard. error code: " << ret2 << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            mGrabbed = true;
        }

        XNextEvent(display, &event);

        // 处理鼠标事件
        if (event.type == MotionNotify) {
            std::cout << "Mouse moved to (" << event.xmotion.x << ", " << event.xmotion.y << ")."
                      << std::endl;
        }
        else if (event.type == ButtonPress) {
            std::cout << "Mouse button pressed." << std::endl;
        }
        else if (event.type == ButtonRelease) {
            std::cout << "Mouse button released." << std::endl;
        }

        // 处理键盘事件
        if (event.type == KeyPress) {
            char   buffer[1];
            KeySym keysym;
            XLookupString(&event.xkey, buffer, sizeof(buffer), &keysym, nullptr);
            std::cout << "Key pressed: " << buffer[0] << " sysem code: " << keysym << std::endl;
            if (keysym == XK_Escape) {
                break;
            }
        }
        else if (event.type == KeyRelease) {
            std::cout << "Key released." << std::endl;
        }

        if (event.type == SelectionNotify) {
            clipboardCallback((XPointer)&event);
            std::cout << "Selection changed." << std::endl;
        }

        std::cout << "Event type: " << event.type << std::endl;
    }

    // 释放抓取
    XUngrabPointer(display, CurrentTime);
    XUngrabKeyboard(display, CurrentTime);

    // 清理
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return 0;
}