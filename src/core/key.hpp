#pragma once

#include "preinclude.hpp"
#include "refl/formatter.hpp"
#include <type_traits>
#include <cstdint>
#include <format>

MKS_BEGIN
/**
 * USB HID Keyboard scan codes as per USB spec 1.11
 * plus some additional codes
 * 
 * Created by MightyPork, 2016
 * Public domain
 * 
 * Adapted from:
 * https://source.android.com/devices/input/keyboard-devices.html
 */
// https://gist.github.com/MightyPork/6da26e382a7ad91b5496ee55fdc73db2


/**
 * Modifier masks - used for the first byte in the HID report.
 * NOTE: The second byte in the report is reserved, 0x00
 */
enum class KeyModifier : uint8_t {
    None       = 0x00,
    LeftCtrl   = 0x01,
    LeftShift  = 0x02,
    LeftAlt    = 0x04,
    LeftMeta   = 0x08,
    RightCtrl  = 0x10,
    RightShift = 0x20,
    RightAlt   = 0x40,
    RightMeta  = 0x80
};
FLAGS_FORMATTER(KeyModifier);

/**
 * Scan codes - last N slots in the HID report (usually 6).
 * 0x00 if no key pressed.
 * 
 * If more than N keys are pressed, the HID reports 
 * ErrorOverflow in all slots to indicate this condition.
 */
enum class Key : uint32_t {
    None = 0x00,              // No key pressed
    ErrorOverflow = 0x01,     // Keyboard Error Roll Over - used for all slots if too many keys are pressed ("Phantom key")
    // KeyboardPostFail = 0x02, // Keyboard POST Fail
    // KeyboardErrorUndefined = 0x03, // Keyboard Error Undefined
    
    A = 0x04,                 // Keyboard a and A
    B = 0x05,                 // Keyboard b and B
    C = 0x06,                 // Keyboard c and C
    D = 0x07,                 // Keyboard d and D
    E = 0x08,                 // Keyboard e and E
    F = 0x09,                 // Keyboard f and F
    G = 0x0a,                 // Keyboard g and G
    H = 0x0b,                 // Keyboard h and H
    I = 0x0c,                 // Keyboard i and I
    J = 0x0d,                 // Keyboard j and J
    K = 0x0e,                 // Keyboard k and K
    L = 0x0f,                 // Keyboard l and L
    M = 0x10,                 // Keyboard m and M
    N = 0x11,                 // Keyboard n and N
    O = 0x12,                 // Keyboard o and O
    P = 0x13,                 // Keyboard p and P
    Q = 0x14,                 // Keyboard q and Q
    R = 0x15,                 // Keyboard r and R
    S = 0x16,                 // Keyboard s and S
    T = 0x17,                 // Keyboard t and T
    U = 0x18,                 // Keyboard u and U
    V = 0x19,                 // Keyboard v and V
    W = 0x1a,                 // Keyboard w and W
    X = 0x1b,                 // Keyboard x and X
    Y = 0x1c,                 // Keyboard y and Y
    Z = 0x1d,                 // Keyboard z and Z

    Digit1 = 0x1e,            // Keyboard 1 and !
    Digit2 = 0x1f,            // Keyboard 2 and @
    Digit3 = 0x20,            // Keyboard 3 and #
    Digit4 = 0x21,            // Keyboard 4 and $
    Digit5 = 0x22,            // Keyboard 5 and %
    Digit6 = 0x23,            // Keyboard 6 and ^
    Digit7 = 0x24,            // Keyboard 7 and &
    Digit8 = 0x25,            // Keyboard 8 and *
    Digit9 = 0x26,            // Keyboard 9 and (
    Digit0 = 0x27,            // Keyboard 0 and )

    Enter = 0x28,             // Keyboard Return (ENTER)
    Esc = 0x29,               // Keyboard ESCAPE
    Backspace = 0x2a,         // Keyboard DELETE (Backspace)
    Tab = 0x2b,               // Keyboard Tab
    Space = 0x2c,             // Keyboard Spacebar
    Minus = 0x2d,             // Keyboard - and _
    Equal = 0x2e,             // Keyboard = and +
    LeftBrace = 0x2f,         // Keyboard [ and {
    RightBrace = 0x30,        // Keyboard ] and }
    Backslash = 0x31,         // Keyboard \ and |
    HashTilde = 0x32,         // Keyboard Non-US # and ~
    Semicolon = 0x33,         // Keyboard ; and :
    Apostrophe = 0x34,        // Keyboard ' and "
    Grave = 0x35,             // Keyboard ` and ~
    Comma = 0x36,             // Keyboard , and <
    Dot = 0x37,               // Keyboard . and >
    Slash = 0x38,             // Keyboard / and ?
    CapsLock = 0x39,          // Keyboard Caps Lock

    F1 = 0x3a,                // Keyboard F1
    F2 = 0x3b,                // Keyboard F2
    F3 = 0x3c,                // Keyboard F3
    F4 = 0x3d,                // Keyboard F4
    F5 = 0x3e,                // Keyboard F5
    F6 = 0x3f,                // Keyboard F6
    F7 = 0x40,                // Keyboard F7
    F8 = 0x41,                // Keyboard F8
    F9 = 0x42,                // Keyboard F9
    F10 = 0x43,               // Keyboard F10
    F11 = 0x44,               // Keyboard F11
    F12 = 0x45,               // Keyboard F12

    SysRq = 0x46,             // Keyboard Print Screen
    ScrollLock = 0x47,        // Keyboard Scroll Lock
    Pause = 0x48,             // Keyboard Pause
    Insert = 0x49,            // Keyboard Insert
    Home = 0x4a,              // Keyboard Home
    PageUp = 0x4b,            // Keyboard Page Up
    Delete = 0x4c,            // Keyboard Delete Forward
    End = 0x4d,               // Keyboard End
    PageDown = 0x4e,          // Keyboard Page Down
    Right = 0x4f,             // Keyboard Right Arrow
    Left = 0x50,              // Keyboard Left Arrow
    Down = 0x51,              // Keyboard Down Arrow
    Up = 0x52,                // Keyboard Up Arrow

    NumLock = 0x53,           // Keyboard Num Lock and Clear
    KeypadSlash = 0x54,       // Keypad /
    KeypadAsterisk = 0x55,    // Keypad *
    KeypadMinus = 0x56,       // Keypad -
    KeypadPlus = 0x57,        // Keypad +
    KeypadEnter = 0x58,       // Keypad ENTER
    Keypad1 = 0x59,           // Keypad 1 and End
    Keypad2 = 0x5a,           // Keypad 2 and Down Arrow
    Keypad3 = 0x5b,           // Keypad 3 and PageDn
    Keypad4 = 0x5c,           // Keypad 4 and Left Arrow
    Keypad5 = 0x5d,           // Keypad 5
    Keypad6 = 0x5e,           // Keypad 6 and Right Arrow
    Keypad7 = 0x5f,           // Keypad 7 and Home
    Keypad8 = 0x60,           // Keypad 8 and Up Arrow
    Keypad9 = 0x61,           // Keypad 9 and Page Up
    Keypad0 = 0x62,           // Keypad 0 and Insert
    KeypadDot = 0x63,         // Keypad . and Delete

    Key102nd = 0x64,          // Keyboard Non-US \ and |
    Compose = 0x65,           // Keyboard Application
    Power = 0x66,             // Keyboard Power
    KeypadEqual = 0x67,       // Keypad =

    F13 = 0x68,               // Keyboard F13
    F14 = 0x69,               // Keyboard F14
    F15 = 0x6a,               // Keyboard F15
    F16 = 0x6b,               // Keyboard F16
    F17 = 0x6c,               // Keyboard F17
    F18 = 0x6d,               // Keyboard F18
    F19 = 0x6e,               // Keyboard F19
    F20 = 0x6f,               // Keyboard F20
    F21 = 0x70,               // Keyboard F21
    F22 = 0x71,               // Keyboard F22
    F23 = 0x72,               // Keyboard F23
    F24 = 0x73,               // Keyboard F24

    Open = 0x74,              // Keyboard Execute
    Help = 0x75,              // Keyboard Help
    Props = 0x76,             // Keyboard Menu
    Front = 0x77,             // Keyboard Select
    Stop = 0x78,              // Keyboard Stop
    Again = 0x79,             // Keyboard Again
    Undo = 0x7a,              // Keyboard Undo
    Cut = 0x7b,               // Keyboard Cut
    Copy = 0x7c,              // Keyboard Copy
    Paste = 0x7d,             // Keyboard Paste
    Find = 0x7e,              // Keyboard Find
    Mute = 0x7f,              // Keyboard Mute
    VolumeUp = 0x80,          // Keyboard Volume Up
    VolumeDown = 0x81,        // Keyboard Volume Down

    // LockingCapsLock = 0x82,  // Keyboard Locking Caps Lock
    // LockingNumLock = 0x83,   // Keyboard Locking Num Lock
    // LockingScrollLock = 0x84,// Keyboard Locking Scroll Lock
    KeypadComma = 0x85,       // Keypad Comma
    // KeypadEqualSign = 0x86,  // Keypad Equal Sign
    
    Ro = 0x87,                // Keyboard International1
    KatakanaHiragana = 0x88,  // Keyboard International2
    Yen = 0x89,               // Keyboard International3
    Henkan = 0x8a,            // Keyboard International4
    Muhenkan = 0x8b,          // Keyboard International5
    KeypadJpComma = 0x8c,     // Keyboard International6
    // Intl7 = 0x8d,            // Keyboard International7
    // Intl8 = 0x8e,            // Keyboard International8
    // Intl9 = 0x8f,            // Keyboard International9
    
    Hangeul = 0x90,           // Keyboard LANG1
    Hanja = 0x91,             // Keyboard LANG2
    Katakana = 0x92,          // Keyboard LANG3
    Hiragana = 0x93,          // Keyboard LANG4
    ZenkakuHankaku = 0x94,    // Keyboard LANG5
    // Lang6 = 0x95,            // Keyboard LANG6
    // Lang7 = 0x96,            // Keyboard LANG7
    // Lang8 = 0x97,            // Keyboard LANG8
    // Lang9 = 0x98,            // Keyboard LANG9
    // AlternateErase = 0x99,   // Keyboard Alternate Erase
    // SysReqAttention = 0x9a,  // Keyboard SysReq/Attention
    // Cancel = 0x9b,           // Keyboard Cancel
    // Clear = 0x9c,            // Keyboard Clear
    // Prior = 0x9d,            // Keyboard Prior
    // Return = 0x9e,           // Keyboard Return
    // Separator = 0x9f,        // Keyboard Separator
    // Out = 0xa0,              // Keyboard Out
    // Oper = 0xa1,             // Keyboard Oper
    // ClearAgain = 0xa2,       // Keyboard Clear/Again
    // CrSelProps = 0xa3,       // Keyboard CrSel/Props
    // ExSel = 0xa4,            // Keyboard ExSel

    // Keypad00 = 0xb0,         // Keypad 00
    // Keypad000 = 0xb1,        // Keypad 000
    // ThousandsSeparator = 0xb2, // Thousands Separator
    // DecimalSeparator = 0xb3, // Decimal Separator
    // CurrencyUnit = 0xb4,     // Currency Unit
    // CurrencySubUnit = 0xb5,  // Currency Sub-unit
    KeypadLeftParen = 0xb6,   // Keypad (
    KeypadRightParen = 0xb7,  // Keypad )
    // KeypadLeftBrace = 0xb8,  // Keypad {
    // KeypadRightBrace = 0xb9, // Keypad }
    // KeypadTab = 0xba,        // Keypad Tab
    // KeypadBackspace = 0xbb,  // Keypad Backspace
    // KeypadA = 0xbc,          // Keypad A
    // KeypadB = 0xbd,          // Keypad B
    // KeypadC = 0xbe,          // Keypad C
    // KeypadD = 0xbf,          // Keypad D
    // KeypadE = 0xc0,          // Keypad E
    // KeypadF = 0xc1,          // Keypad F
    // KeypadXor = 0xc2,        // Keypad XOR
    // KeypadCaret = 0xc3,      // Keypad ^
    // KeypadPercent = 0xc4,    // Keypad %
    // KeypadLess = 0xc5,       // Keypad <
    // KeypadGreater = 0xc6,    // Keypad >
    // KeypadAmpersand = 0xc7,  // Keypad &
    // KeypadDoubleAmp = 0xc8,  // Keypad &&
    // KeypadPipe = 0xc9,       // Keypad |
    // KeypadDoublePipe = 0xca, // Keypad ||
    // KeypadColon = 0xcb,      // Keypad :
    // KeypadHash = 0xcc,       // Keypad #
    // KeypadSpace = 0xcd,      // Keypad Space
    // KeypadAt = 0xce,         // Keypad @
    // KeypadExclaim = 0xcf,    // Keypad !
    // KeypadMemStore = 0xd0,   // Keypad Memory Store
    // KeypadMemRecall = 0xd1,  // Keypad Memory Recall
    // KeypadMemClear = 0xd2,   // Keypad Memory Clear
    // KeypadMemAdd = 0xd3,     // Keypad Memory Add
    // KeypadMemSub = 0xd4,     // Keypad Memory Subtract
    // KeypadMemMult = 0xd5,    // Keypad Memory Multiply
    // KeypadMemDiv = 0xd6,     // Keypad Memory Divide
    // KeypadPlusMinus = 0xd7,  // Keypad +/-
    // KeypadClear = 0xd8,      // Keypad Clear
    // KeypadClearEntry = 0xd9, // Keypad Clear Entry
    // KeypadBinary = 0xda,     // Keypad Binary
    // KeypadOctal = 0xdb,      // Keypad Octal
    // KeypadDecimal = 0xdc,    // Keypad Decimal
    // KeypadHexadecimal = 0xdd,// Keypad Hexadecimal

    LeftCtrl = 0xe0,          // Keyboard Left Control
    LeftShift = 0xe1,         // Keyboard Left Shift
    LeftAlt = 0xe2,           // Keyboard Left Alt
    LeftMeta = 0xe3,          // Keyboard Left GUI
    RightCtrl = 0xe4,         // Keyboard Right Control
    RightShift = 0xe5,        // Keyboard Right Shift
    RightAlt = 0xe6,          // Keyboard Right Alt
    RightMeta = 0xe7,         // Keyboard Right GUI

    MediaPlayPause = 0xe8,
    MediaStopCd = 0xe9,
    MediaPreviousSong = 0xea,
    MediaNextSong = 0xeb,
    MediaEjectCd = 0xec,
    MediaVolumeUp = 0xed,
    MediaVolumeDown = 0xee,
    MediaMute = 0xef,
    MediaWww = 0xf0,
    MediaBack = 0xf1,
    MediaForward = 0xf2,
    MediaStop = 0xf3,
    MediaFind = 0xf4,
    MediaScrollUp = 0xf5,
    MediaScrollDown = 0xf6,
    MediaEdit = 0xf7,
    MediaSleep = 0xf8,
    MediaCoffee = 0xf9,
    MediaRefresh = 0xfa,
    MediaCalc = 0xfb
};
ENUM_FORMATTER(Key);

// Operators for the KeyModfier
constexpr auto operator |(KeyModifier lhs, KeyModifier rhs) -> KeyModifier {
    return static_cast<KeyModifier>(static_cast<int>(lhs) | static_cast<int>(rhs));
}

constexpr auto operator &(KeyModifier lhs, KeyModifier rhs) -> KeyModifier {
    return static_cast<KeyModifier>(static_cast<int>(lhs) & static_cast<int>(rhs));
}

constexpr auto operator ~(KeyModifier lhs) -> KeyModifier {
    return static_cast<KeyModifier>(~static_cast<int>(lhs));
}

constexpr auto operator |=(KeyModifier &lhs, KeyModifier rhs) -> KeyModifier & {
    lhs = lhs | rhs;
    return lhs;
}

constexpr auto operator &=(KeyModifier &lhs, KeyModifier rhs) -> KeyModifier & {
    lhs = lhs & rhs;
    return lhs;
}

MKS_END