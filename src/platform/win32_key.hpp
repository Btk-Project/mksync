#pragma once

#ifdef _WIN32

#include "preinclude.hpp"
#include "core/key.hpp"

#include <Windows.h>

#include <limits>
#include <optional>

MKS_BEGIN

// MARK: Win32 key translation

// Capture: VK / scan code -> Key (+ modifiers)
// Inject:  Key (+ nativeCode) -> VK for SendInput

struct Win32Key {
    WORD virtualKey = 0;
    bool extended = false;
};

// Left/right modifiers often arrive as generic VK_SHIFT / VK_CONTROL / VK_MENU.
// Expand via scan code so KeyEvent can distinguish LeftCtrl vs RightCtrl, etc.
inline auto normalizeVirtualKey(DWORD vkCode, DWORD scanCode) -> DWORD {
    switch (vkCode) {
        case VK_SHIFT:
        case VK_CONTROL:
        case VK_MENU:
            return ::MapVirtualKeyW(scanCode, MAPVK_VSC_TO_VK_EX);
        default:
            return vkCode;
    }
}

inline auto keyModifierFor(Key key) -> KeyModifier {
    switch (key) {
        case Key::LeftCtrl: return KeyModifier::LeftCtrl;
        case Key::RightCtrl: return KeyModifier::RightCtrl;
        case Key::LeftShift: return KeyModifier::LeftShift;
        case Key::RightShift: return KeyModifier::RightShift;
        case Key::LeftAlt: return KeyModifier::LeftAlt;
        case Key::RightAlt: return KeyModifier::RightAlt;
        case Key::LeftMeta: return KeyModifier::LeftMeta;
        case Key::RightMeta: return KeyModifier::RightMeta;
        default: return KeyModifier::None;
    }
}

inline auto currentModifiers() -> KeyModifier {
    auto down = [](int vk) {
        return (::GetAsyncKeyState(vk) & 0x8000) != 0;
    };

    auto modifiers = KeyModifier::None;
    if (down(VK_LCONTROL)) { modifiers |= KeyModifier::LeftCtrl; }
    if (down(VK_RCONTROL)) { modifiers |= KeyModifier::RightCtrl; }
    if (down(VK_LSHIFT))   { modifiers |= KeyModifier::LeftShift; }
    if (down(VK_RSHIFT))   { modifiers |= KeyModifier::RightShift; }
    if (down(VK_LMENU))    { modifiers |= KeyModifier::LeftAlt; }
    if (down(VK_RMENU))    { modifiers |= KeyModifier::RightAlt; }
    if (down(VK_LWIN))     { modifiers |= KeyModifier::LeftMeta; }
    if (down(VK_RWIN))     { modifiers |= KeyModifier::RightMeta; }
    return modifiers;
}

inline auto isExtendedKey(Key key) -> bool {
    switch (key) {
        case Key::RightCtrl:
        case Key::RightAlt:
        case Key::Insert:
        case Key::Delete:
        case Key::Home:
        case Key::End:
        case Key::PageUp:
        case Key::PageDown:
        case Key::Right:
        case Key::Left:
        case Key::Down:
        case Key::Up:
        case Key::KeypadSlash:
        case Key::KeypadEnter:
            return true;
        default:
            return false;
    }
}

inline auto virtualKeyToKey(DWORD vkCode) -> Key {
    using enum Key;
    if (vkCode >= 'A' && vkCode <= 'Z') {
        return static_cast<Key>(static_cast<uint32_t>(A) + (vkCode - 'A'));
    }
    if (vkCode >= '0' && vkCode <= '9') {
        return static_cast<Key>(static_cast<uint32_t>(Digit0) + (vkCode - '0'));
    }

    switch (vkCode) {
        case VK_RETURN: return Enter;
        case VK_ESCAPE: return Esc;
        case VK_BACK: return Backspace;
        case VK_TAB: return Tab;
        case VK_SPACE: return Space;
        case VK_OEM_MINUS: return Minus;
        case VK_OEM_PLUS: return Equal;
        case VK_OEM_4: return LeftBrace;
        case VK_OEM_6: return RightBrace;
        case VK_OEM_5: return Backslash;
        case VK_OEM_1: return Semicolon;
        case VK_OEM_7: return Apostrophe;
        case VK_OEM_3: return Grave;
        case VK_OEM_COMMA: return Comma;
        case VK_OEM_PERIOD: return Dot;
        case VK_OEM_2: return Slash;
        case VK_CAPITAL: return CapsLock;
        case VK_F1: return F1;
        case VK_F2: return F2;
        case VK_F3: return F3;
        case VK_F4: return F4;
        case VK_F5: return F5;
        case VK_F6: return F6;
        case VK_F7: return F7;
        case VK_F8: return F8;
        case VK_F9: return F9;
        case VK_F10: return F10;
        case VK_F11: return F11;
        case VK_F12: return F12;
        case VK_SNAPSHOT: return SysRq;
        case VK_SCROLL: return ScrollLock;
        case VK_PAUSE: return Pause;
        case VK_INSERT: return Insert;
        case VK_HOME: return Home;
        case VK_PRIOR: return PageUp;
        case VK_DELETE: return Delete;
        case VK_END: return End;
        case VK_NEXT: return PageDown;
        case VK_RIGHT: return Right;
        case VK_LEFT: return Left;
        case VK_DOWN: return Down;
        case VK_UP: return Up;
        case VK_NUMLOCK: return NumLock;
        case VK_DIVIDE: return KeypadSlash;
        case VK_MULTIPLY: return KeypadAsterisk;
        case VK_SUBTRACT: return KeypadMinus;
        case VK_ADD: return KeypadPlus;
        case VK_NUMPAD0: return Keypad0;
        case VK_NUMPAD1: return Keypad1;
        case VK_NUMPAD2: return Keypad2;
        case VK_NUMPAD3: return Keypad3;
        case VK_NUMPAD4: return Keypad4;
        case VK_NUMPAD5: return Keypad5;
        case VK_NUMPAD6: return Keypad6;
        case VK_NUMPAD7: return Keypad7;
        case VK_NUMPAD8: return Keypad8;
        case VK_NUMPAD9: return Keypad9;
        case VK_DECIMAL: return KeypadDot;
        case VK_LCONTROL: return LeftCtrl;
        case VK_RCONTROL: return RightCtrl;
        case VK_LSHIFT: return LeftShift;
        case VK_RSHIFT: return RightShift;
        case VK_LMENU: return LeftAlt;
        case VK_RMENU: return RightAlt;
        case VK_LWIN: return LeftMeta;
        case VK_RWIN: return RightMeta;
        default: return Key::None;
    }
}

inline auto keyToWin32(Key key, uint32_t nativeCode = 0) -> std::optional<Win32Key> {
    using enum Key;

    if (key >= A && key <= Z) {
        return Win32Key {
            .virtualKey = static_cast<WORD>('A' + (static_cast<uint32_t>(key) - static_cast<uint32_t>(A))),
        };
    }
    if (key >= Digit1 && key <= Digit9) {
        return Win32Key {
            .virtualKey = static_cast<WORD>('1' + (static_cast<uint32_t>(key) - static_cast<uint32_t>(Digit1))),
        };
    }
    if (key >= F1 && key <= F12) {
        return Win32Key {
            .virtualKey = static_cast<WORD>(VK_F1 + (static_cast<uint32_t>(key) - static_cast<uint32_t>(F1))),
        };
    }
    if (key >= F13 && key <= F24) {
        return Win32Key {
            .virtualKey = static_cast<WORD>(VK_F13 + (static_cast<uint32_t>(key) - static_cast<uint32_t>(F13))),
        };
    }
    if (key >= Keypad1 && key <= Keypad9) {
        return Win32Key {
            .virtualKey = static_cast<WORD>(VK_NUMPAD1 + (static_cast<uint32_t>(key) - static_cast<uint32_t>(Keypad1))),
        };
    }

    auto virtualKey = WORD {};
    switch (key) {
        case Digit0: virtualKey = '0'; break;
        case Enter: virtualKey = VK_RETURN; break;
        case Esc: virtualKey = VK_ESCAPE; break;
        case Backspace: virtualKey = VK_BACK; break;
        case Tab: virtualKey = VK_TAB; break;
        case Space: virtualKey = VK_SPACE; break;
        case Minus: virtualKey = VK_OEM_MINUS; break;
        case Equal: virtualKey = VK_OEM_PLUS; break;
        case LeftBrace: virtualKey = VK_OEM_4; break;
        case RightBrace: virtualKey = VK_OEM_6; break;
        case Backslash: virtualKey = VK_OEM_5; break;
        case Semicolon: virtualKey = VK_OEM_1; break;
        case Apostrophe: virtualKey = VK_OEM_7; break;
        case Grave: virtualKey = VK_OEM_3; break;
        case Comma: virtualKey = VK_OEM_COMMA; break;
        case Dot: virtualKey = VK_OEM_PERIOD; break;
        case Slash: virtualKey = VK_OEM_2; break;
        case CapsLock: virtualKey = VK_CAPITAL; break;
        case SysRq: virtualKey = VK_SNAPSHOT; break;
        case ScrollLock: virtualKey = VK_SCROLL; break;
        case Pause: virtualKey = VK_PAUSE; break;
        case Insert: virtualKey = VK_INSERT; break;
        case Home: virtualKey = VK_HOME; break;
        case PageUp: virtualKey = VK_PRIOR; break;
        case Delete: virtualKey = VK_DELETE; break;
        case End: virtualKey = VK_END; break;
        case PageDown: virtualKey = VK_NEXT; break;
        case Right: virtualKey = VK_RIGHT; break;
        case Left: virtualKey = VK_LEFT; break;
        case Down: virtualKey = VK_DOWN; break;
        case Up: virtualKey = VK_UP; break;
        case NumLock: virtualKey = VK_NUMLOCK; break;
        case KeypadSlash: virtualKey = VK_DIVIDE; break;
        case KeypadAsterisk: virtualKey = VK_MULTIPLY; break;
        case KeypadMinus: virtualKey = VK_SUBTRACT; break;
        case KeypadPlus: virtualKey = VK_ADD; break;
        case KeypadEnter: virtualKey = VK_RETURN; break;
        case Keypad0: virtualKey = VK_NUMPAD0; break;
        case KeypadDot: virtualKey = VK_DECIMAL; break;
        case LeftCtrl: virtualKey = VK_LCONTROL; break;
        case LeftShift: virtualKey = VK_LSHIFT; break;
        case LeftAlt: virtualKey = VK_LMENU; break;
        case LeftMeta: virtualKey = VK_LWIN; break;
        case RightCtrl: virtualKey = VK_RCONTROL; break;
        case RightShift: virtualKey = VK_RSHIFT; break;
        case RightAlt: virtualKey = VK_RMENU; break;
        case RightMeta: virtualKey = VK_RWIN; break;
        case VolumeUp:
        case MediaVolumeUp: virtualKey = VK_VOLUME_UP; break;
        case VolumeDown:
        case MediaVolumeDown: virtualKey = VK_VOLUME_DOWN; break;
        case Mute:
        case MediaMute: virtualKey = VK_VOLUME_MUTE; break;
        case MediaPlayPause: virtualKey = VK_MEDIA_PLAY_PAUSE; break;
        case MediaStopCd: virtualKey = VK_MEDIA_STOP; break;
        case MediaPreviousSong: virtualKey = VK_MEDIA_PREV_TRACK; break;
        case MediaNextSong: virtualKey = VK_MEDIA_NEXT_TRACK; break;
        default:
            if (nativeCode == 0 || nativeCode > std::numeric_limits<WORD>::max()) {
                return std::nullopt;
            }
            return Win32Key {
                .virtualKey = static_cast<WORD>(nativeCode),
                .extended = isExtendedKey(key),
            };
    }

    return Win32Key {
        .virtualKey = virtualKey,
        .extended = isExtendedKey(key),
    };
}

MKS_END

#endif // _WIN32
