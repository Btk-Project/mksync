#include <Windows.h>
#include <iostream>

class EventListener {
public:
    EventListener();
    ~EventListener();
private:
    LRESULT mouseProc(int ncode, WPARAM wp, LPARAM lp);
    LRESULT keyboardProc(int ncode, WPARAM wp, LPARAM lp);

    HHOOK mMosueHook;
    HHOOK mKeyboardHook;    
};

EventListener::EventListener() {
    static EventListener *self = this;
    mMosueHook = SetWindowsHookExW(WH_MOUSE_LL, [](int ncode, WPARAM wp, LPARAM lp) {
        return self->mouseProc(ncode, wp, lp);
    }, nullptr, 0);
    if (mMosueHook == nullptr) {
        std::cout << "SetWindowsHookExW failed: " << GetLastError() << std::endl;
        return;
    }

    mKeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, [](int ncode, WPARAM wp, LPARAM lp) {
        return self->keyboardProc(ncode, wp, lp);
    }, nullptr, 0);
    if (mKeyboardHook == nullptr) {
        std::cout << "SetWindowsHookExW failed: " << GetLastError() << std::endl;
        return;
    }
}

EventListener::~EventListener() {
    UnhookWindowsHookEx(mMosueHook);
    UnhookWindowsHookEx(mKeyboardHook);
}

LRESULT EventListener::mouseProc(int ncode, WPARAM wp, LPARAM lp) {
    auto hookStruct = reinterpret_cast<MSLLHOOKSTRUCT*>(lp);
    switch (wp) {
        case WM_LBUTTONDOWN:
            std::cout << "Left Mouse Button Down" << std::endl;
            break;
        case WM_LBUTTONUP:
            std::cout << "Left Mouse Button Up" << std::endl;
            break;
        case WM_RBUTTONDOWN:
            std::cout << "Right Mouse Button Down" << std::endl;
            break;
        case WM_RBUTTONUP:
            std::cout << "Right Mouse Button Up" << std::endl;
            break;
        case WM_MOUSEMOVE:
            std::cout << "Mouse Move X: " << hookStruct->pt.x << " Y: " << hookStruct->pt.y << std::endl;
            break;
        case WM_MOUSEWHEEL:
            std::cout << "Mouse Wheel Delta: " << HIWORD(hookStruct->mouseData) << std::endl;
    }
    return CallNextHookEx(nullptr, ncode, wp, lp);
}

LRESULT EventListener::keyboardProc(int ncode, WPARAM wp, LPARAM lp) {
    auto hookStruct = reinterpret_cast<KBDLLHOOKSTRUCT*>(lp);
    switch (wp) {
        case WM_KEYDOWN:
            std::cout << "Key Down ";
            break;
        case WM_KEYUP:
            std::cout << "Key Up ";
    }
    char name[256] {0};
    GetKeyNameTextA(hookStruct->scanCode << 16, name, 256);
    std::cout << "Key: " << hookStruct->vkCode << " Name: " << name << std::endl;
    return CallNextHookEx(nullptr, ncode, wp, lp);
}

int main() {
    EventListener listener;
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}