#pragma once

#include "core/key.hpp"
#include "preinclude.hpp"

#include <linux/input-event-codes.h>

#include <cstdint>
#include <optional>

MKS_BEGIN

namespace wayland
{

    // zwp_virtual_keyboard_v1.key uses Linux input-event key codes. InputEvent::key
    // is a USB HID usage, so nativeCode cannot be forwarded: it belongs to the
    // capture host.
    constexpr auto evdevKeyCode(Key key) -> std::optional<uint32_t>
    {
        using enum Key;
        switch (key) {
        case A:
            return KEY_A;
        case B:
            return KEY_B;
        case C:
            return KEY_C;
        case D:
            return KEY_D;
        case E:
            return KEY_E;
        case F:
            return KEY_F;
        case G:
            return KEY_G;
        case H:
            return KEY_H;
        case I:
            return KEY_I;
        case J:
            return KEY_J;
        case K:
            return KEY_K;
        case L:
            return KEY_L;
        case M:
            return KEY_M;
        case N:
            return KEY_N;
        case O:
            return KEY_O;
        case P:
            return KEY_P;
        case Q:
            return KEY_Q;
        case R:
            return KEY_R;
        case S:
            return KEY_S;
        case T:
            return KEY_T;
        case U:
            return KEY_U;
        case V:
            return KEY_V;
        case W:
            return KEY_W;
        case X:
            return KEY_X;
        case Y:
            return KEY_Y;
        case Z:
            return KEY_Z;

        case Digit1:
            return KEY_1;
        case Digit2:
            return KEY_2;
        case Digit3:
            return KEY_3;
        case Digit4:
            return KEY_4;
        case Digit5:
            return KEY_5;
        case Digit6:
            return KEY_6;
        case Digit7:
            return KEY_7;
        case Digit8:
            return KEY_8;
        case Digit9:
            return KEY_9;
        case Digit0:
            return KEY_0;

        case Enter:
            return KEY_ENTER;
        case Esc:
            return KEY_ESC;
        case Backspace:
            return KEY_BACKSPACE;
        case Tab:
            return KEY_TAB;
        case Space:
            return KEY_SPACE;
        case Minus:
            return KEY_MINUS;
        case Equal:
            return KEY_EQUAL;
        case LeftBrace:
            return KEY_LEFTBRACE;
        case RightBrace:
            return KEY_RIGHTBRACE;
        case Backslash:
            return KEY_BACKSLASH;
        case HashTilde:
            return KEY_102ND;
        case Semicolon:
            return KEY_SEMICOLON;
        case Apostrophe:
            return KEY_APOSTROPHE;
        case Grave:
            return KEY_GRAVE;
        case Comma:
            return KEY_COMMA;
        case Dot:
            return KEY_DOT;
        case Slash:
            return KEY_SLASH;
        case CapsLock:
            return KEY_CAPSLOCK;

        case F1:
            return KEY_F1;
        case F2:
            return KEY_F2;
        case F3:
            return KEY_F3;
        case F4:
            return KEY_F4;
        case F5:
            return KEY_F5;
        case F6:
            return KEY_F6;
        case F7:
            return KEY_F7;
        case F8:
            return KEY_F8;
        case F9:
            return KEY_F9;
        case F10:
            return KEY_F10;
        case F11:
            return KEY_F11;
        case F12:
            return KEY_F12;
        case F13:
            return KEY_F13;
        case F14:
            return KEY_F14;
        case F15:
            return KEY_F15;
        case F16:
            return KEY_F16;
        case F17:
            return KEY_F17;
        case F18:
            return KEY_F18;
        case F19:
            return KEY_F19;
        case F20:
            return KEY_F20;
        case F21:
            return KEY_F21;
        case F22:
            return KEY_F22;
        case F23:
            return KEY_F23;
        case F24:
            return KEY_F24;

        case SysRq:
            return KEY_SYSRQ;
        case ScrollLock:
            return KEY_SCROLLLOCK;
        case Pause:
            return KEY_PAUSE;
        case Insert:
            return KEY_INSERT;
        case Home:
            return KEY_HOME;
        case PageUp:
            return KEY_PAGEUP;
        case Delete:
            return KEY_DELETE;
        case End:
            return KEY_END;
        case PageDown:
            return KEY_PAGEDOWN;
        case Right:
            return KEY_RIGHT;
        case Left:
            return KEY_LEFT;
        case Down:
            return KEY_DOWN;
        case Up:
            return KEY_UP;

        case NumLock:
            return KEY_NUMLOCK;
        case KeypadSlash:
            return KEY_KPSLASH;
        case KeypadAsterisk:
            return KEY_KPASTERISK;
        case KeypadMinus:
            return KEY_KPMINUS;
        case KeypadPlus:
            return KEY_KPPLUS;
        case KeypadEnter:
            return KEY_KPENTER;
        case Keypad1:
            return KEY_KP1;
        case Keypad2:
            return KEY_KP2;
        case Keypad3:
            return KEY_KP3;
        case Keypad4:
            return KEY_KP4;
        case Keypad5:
            return KEY_KP5;
        case Keypad6:
            return KEY_KP6;
        case Keypad7:
            return KEY_KP7;
        case Keypad8:
            return KEY_KP8;
        case Keypad9:
            return KEY_KP9;
        case Keypad0:
            return KEY_KP0;
        case KeypadDot:
            return KEY_KPDOT;
        case Key102nd:
            return KEY_102ND;
        case Compose:
            return KEY_COMPOSE;
        case Power:
            return KEY_POWER;
        case KeypadEqual:
            return KEY_KPEQUAL;
        case KeypadComma:
            return KEY_KPCOMMA;

        case Open:
            return KEY_OPEN;
        case Help:
            return KEY_HELP;
        case Props:
            return KEY_PROPS;
        case Front:
            return KEY_FRONT;
        case Stop:
            return KEY_STOP;
        case Again:
            return KEY_AGAIN;
        case Undo:
            return KEY_UNDO;
        case Cut:
            return KEY_CUT;
        case Copy:
            return KEY_COPY;
        case Paste:
            return KEY_PASTE;
        case Find:
            return KEY_FIND;

        case Ro:
            return KEY_RO;
        case KatakanaHiragana:
            return KEY_KATAKANAHIRAGANA;
        case Yen:
            return KEY_YEN;
        case Henkan:
            return KEY_HENKAN;
        case Muhenkan:
            return KEY_MUHENKAN;
        case KeypadJpComma:
            return KEY_KPJPCOMMA;
        case Hangeul:
            return KEY_HANGEUL;
        case Hanja:
            return KEY_HANJA;
        case Katakana:
            return KEY_KATAKANA;
        case Hiragana:
            return KEY_HIRAGANA;
        case ZenkakuHankaku:
            return KEY_ZENKAKUHANKAKU;

        case LeftCtrl:
            return KEY_LEFTCTRL;
        case RightCtrl:
            return KEY_RIGHTCTRL;
        case LeftShift:
            return KEY_LEFTSHIFT;
        case RightShift:
            return KEY_RIGHTSHIFT;
        case LeftAlt:
            return KEY_LEFTALT;
        case RightAlt:
            return KEY_RIGHTALT;
        case LeftMeta:
            return KEY_LEFTMETA;
        case RightMeta:
            return KEY_RIGHTMETA;

        case Mute:
        case MediaMute:
            return KEY_MUTE;
        case VolumeUp:
        case MediaVolumeUp:
            return KEY_VOLUMEUP;
        case VolumeDown:
        case MediaVolumeDown:
            return KEY_VOLUMEDOWN;
        case MediaPlayPause:
            return KEY_PLAYPAUSE;
        case MediaStopCd:
            return KEY_STOPCD;
        case MediaPreviousSong:
            return KEY_PREVIOUSSONG;
        case MediaNextSong:
            return KEY_NEXTSONG;
        case MediaEjectCd:
            return KEY_EJECTCD;
        case MediaWww:
            return KEY_WWW;
        case MediaBack:
            return KEY_BACK;
        case MediaForward:
            return KEY_FORWARD;
        case MediaStop:
            return KEY_STOP;
        case MediaFind:
            return KEY_FIND;
        case MediaScrollUp:
            return KEY_SCROLLUP;
        case MediaScrollDown:
            return KEY_SCROLLDOWN;
        case MediaEdit:
            return KEY_EDIT;
        case MediaSleep:
            return KEY_SLEEP;
        case MediaCoffee:
            return KEY_COFFEE;
        case MediaRefresh:
            return KEY_REFRESH;
        case MediaCalc:
            return KEY_CALC;

        default:
            return std::nullopt;
        }
    }

    constexpr auto keyFromEvdev(uint32_t keyCode) -> Key
    {
        // Key is a USB HID usage with a compact range in mksync. Keeping the
        // reverse map derived from evdevKeyCode prevents the capture and
        // injection tables from drifting apart.
        for (auto usage = uint32_t{0}; usage <= static_cast<uint32_t>(Key::MediaCalc); ++usage) {
            const auto key    = static_cast<Key>(usage);
            const auto native = evdevKeyCode(key);
            if (native && *native == keyCode) {
                return key;
            }
        }
        return Key::None;
    }

} // namespace wayland

MKS_END
