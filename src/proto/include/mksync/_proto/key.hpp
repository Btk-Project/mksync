#pragma once
#include <mksync/_proto/_config.hpp>
// Standard Library
#include <cstdint>
// System Library
// Third-Party Library
// Local Library

MKS_BEGIN
MKS_PROTO_BEGIN

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
enum class KeyModifier : uint16_t
{
    eNone       = 0x0000,
    eLeftCtrl   = 0x0001,
    eLeftShift  = 0x0002,
    eLeftAlt    = 0x0004,
    eLeftMeta   = 0x0008,
    eRightCtrl  = 0x0010,
    eRightShift = 0x0020,
    eRightAlt   = 0x0040,
    eRightMeta  = 0x0080,
    eUnused1    = 0x0100,
};

enum class KeyLock : uint8_t
{
    eNone       = 0x00,
    eCapsLock   = 0x01,
    eNumLock    = 0x02,
    eScrollLock = 0x04,
    eFnLock     = 0x08,
    eLevel5Lock = 0x10,
};

/**
 * Scan codes - last N slots in the HID report (usually 6).
 * 0x00 if no key pressed.
 *
 * If more than N keys are pressed, the HID reports
 * ErrorOverflow in all slots to indicate this condition.
 */
enum class KeyCode : uint8_t
{
    eNone          = 0x00, // No key pressed
    eErrorOverflow = 0x01, // Keyboard Error Roll Over - used for all slots if too many keys are
                           // pressed ("Phantom key")
    // eKeyboardPostFail       = 0x02, // Keyboard POST Fail
    // eKeyboardErrorUndefined = 0x03, // Keyboard Error Undefined

    eA = 0x04, // Keyboard a and A
    eB = 0x05, // Keyboard b and B
    eC = 0x06, // Keyboard c and C
    eD = 0x07, // Keyboard d and D
    eE = 0x08, // Keyboard e and E
    eF = 0x09, // Keyboard f and F
    eG = 0x0a, // Keyboard g and G
    eH = 0x0b, // Keyboard h and H
    eI = 0x0c, // Keyboard i and I
    eJ = 0x0d, // Keyboard j and J
    eK = 0x0e, // Keyboard k and K
    eL = 0x0f, // Keyboard l and L
    eM = 0x10, // Keyboard m and M
    eN = 0x11, // Keyboard n and N
    eO = 0x12, // Keyboard o and O
    eP = 0x13, // Keyboard p and P
    eQ = 0x14, // Keyboard q and Q
    eR = 0x15, // Keyboard r and R
    eS = 0x16, // Keyboard s and S
    eT = 0x17, // Keyboard t and T
    eU = 0x18, // Keyboard u and U
    eV = 0x19, // Keyboard v and V
    eW = 0x1a, // Keyboard w and W
    eX = 0x1b, // Keyboard x and X
    eY = 0x1c, // Keyboard y and Y
    eZ = 0x1d, // Keyboard z and Z

    eDigit1 = 0x1e, // Keyboard 1 and !
    eDigit2 = 0x1f, // Keyboard 2 and @
    eDigit3 = 0x20, // Keyboard 3 and #
    eDigit4 = 0x21, // Keyboard 4 and $
    eDigit5 = 0x22, // Keyboard 5 and %
    eDigit6 = 0x23, // Keyboard 6 and ^
    eDigit7 = 0x24, // Keyboard 7 and &
    eDigit8 = 0x25, // Keyboard 8 and *
    eDigit9 = 0x26, // Keyboard 9 and (
    eDigit0 = 0x27, // Keyboard 0 and )

    eEnter      = 0x28, // Keyboard Return (ENTER)
    eEsc        = 0x29, // Keyboard ESCAPE
    eBackspace  = 0x2a, // Keyboard DELETE (Backspace)
    eTab        = 0x2b, // Keyboard Tab
    eSpace      = 0x2c, // Keyboard Spacebar
    eMinus      = 0x2d, // Keyboard - and _
    eEqual      = 0x2e, // Keyboard = and +
    eLeftBrace  = 0x2f, // Keyboard [ and {
    eRightBrace = 0x30, // Keyboard ] and }
    eBackslash  = 0x31, // Keyboard \ and |
    eHashTilde  = 0x32, // Keyboard Non-US # and ~
    eSemicolon  = 0x33, // Keyboard ; and :
    eApostrophe = 0x34, // Keyboard ' and "
    eGrave      = 0x35, // Keyboard ` and ~
    eComma      = 0x36, // Keyboard , and <
    eDot        = 0x37, // Keyboard . and >
    eSlash      = 0x38, // Keyboard / and ?
    eCapsLock   = 0x39, // Keyboard Caps Lock

    eF1  = 0x3a, // Keyboard F1
    eF2  = 0x3b, // Keyboard F2
    eF3  = 0x3c, // Keyboard F3
    eF4  = 0x3d, // Keyboard F4
    eF5  = 0x3e, // Keyboard F5
    eF6  = 0x3f, // Keyboard F6
    eF7  = 0x40, // Keyboard F7
    eF8  = 0x41, // Keyboard F8
    eF9  = 0x42, // Keyboard F9
    eF10 = 0x43, // Keyboard F10
    eF11 = 0x44, // Keyboard F11
    eF12 = 0x45, // Keyboard F12

    eSysRq      = 0x46, // Keyboard Print Screen
    eScrollLock = 0x47, // Keyboard Scroll Lock
    ePause      = 0x48, // Keyboard Pause
    eInsert     = 0x49, // Keyboard Insert
    eHome       = 0x4a, // Keyboard Home
    ePageUp     = 0x4b, // Keyboard Page Up
    eDelete     = 0x4c, // Keyboard Delete Forward
    eEnd        = 0x4d, // Keyboard End
    ePageDown   = 0x4e, // Keyboard Page Down
    eRight      = 0x4f, // Keyboard Right Arrow
    eLeft       = 0x50, // Keyboard Left Arrow
    eDown       = 0x51, // Keyboard Down Arrow
    eUp         = 0x52, // Keyboard Up Arrow

    eNumLock        = 0x53, // Keyboard Num Lock and Clear
    eKeypadSlash    = 0x54, // Keypad /
    eKeypadAsterisk = 0x55, // Keypad *
    eKeypadMinus    = 0x56, // Keypad -
    eKeypadPlus     = 0x57, // Keypad +
    eKeypadEnter    = 0x58, // Keypad ENTER
    eKeypad1        = 0x59, // Keypad 1 and End
    eKeypad2        = 0x5a, // Keypad 2 and Down Arrow
    eKeypad3        = 0x5b, // Keypad 3 and PageDn
    eKeypad4        = 0x5c, // Keypad 4 and Left Arrow
    eKeypad5        = 0x5d, // Keypad 5
    eKeypad6        = 0x5e, // Keypad 6 and Right Arrow
    eKeypad7        = 0x5f, // Keypad 7 and Home
    eKeypad8        = 0x60, // Keypad 8 and Up Arrow
    eKeypad9        = 0x61, // Keypad 9 and Page Up
    eKeypad0        = 0x62, // Keypad 0 and Insert
    eKeypadDot      = 0x63, // Keypad . and Delete

    eKey102nd    = 0x64, // Keyboard Non-US \ and |
    eCompose     = 0x65, // Keyboard Application
    ePower       = 0x66, // Keyboard Power
    eKeypadEqual = 0x67, // Keypad =

    eF13 = 0x68, // Keyboard F13
    eF14 = 0x69, // Keyboard F14
    eF15 = 0x6a, // Keyboard F15
    eF16 = 0x6b, // Keyboard F16
    eF17 = 0x6c, // Keyboard F17
    eF18 = 0x6d, // Keyboard F18
    eF19 = 0x6e, // Keyboard F19
    eF20 = 0x6f, // Keyboard F20
    eF21 = 0x70, // Keyboard F21
    eF22 = 0x71, // Keyboard F22
    eF23 = 0x72, // Keyboard F23
    eF24 = 0x73, // Keyboard F24

    eOpen       = 0x74, // Keyboard Execute
    eHelp       = 0x75, // Keyboard Help
    eProps      = 0x76, // Keyboard Menu
    eFront      = 0x77, // Keyboard Select
    eStop       = 0x78, // Keyboard Stop
    eAgain      = 0x79, // Keyboard Again
    eUndo       = 0x7a, // Keyboard Undo
    eCut        = 0x7b, // Keyboard Cut
    eCopy       = 0x7c, // Keyboard Copy
    ePaste      = 0x7d, // Keyboard Paste
    eFind       = 0x7e, // Keyboard Find
    eMute       = 0x7f, // Keyboard Mute
    eVolumeUp   = 0x80, // Keyboard Volume Up
    eVolumeDown = 0x81, // Keyboard Volume Down

    // eLockingCapsLock = 0x82,  // Keyboard Locking Caps Lock
    // eLockingNumLock = 0x83,   // Keyboard Locking Num Lock
    // eLockingScrollLock = 0x84,// Keyboard Locking Scroll Lock
    eKeypadComma = 0x85, // Keypad Comma
    // eKeypadEqualSign = 0x86,  // Keypad Equal Sign

    eRo               = 0x87, // Keyboard International1
    eKatakanaHiragana = 0x88, // Keyboard International2
    eYen              = 0x89, // Keyboard International3
    eHenkan           = 0x8a, // Keyboard International4
    eMuhenkan         = 0x8b, // Keyboard International5
    eKeypadJpComma    = 0x8c, // Keyboard International6
    // eIntl7 = 0x8d,            // Keyboard International7
    // eIntl8 = 0x8e,            // Keyboard International8
    // eIntl9 = 0x8f,            // Keyboard International9

    eHangeul        = 0x90, // Keyboard LANG1
    eHanja          = 0x91, // Keyboard LANG2
    eKatakana       = 0x92, // Keyboard LANG3
    eHiragana       = 0x93, // Keyboard LANG4
    eZenkakuHankaku = 0x94, // Keyboard LANG5
    // eLang6 = 0x95,            // Keyboard LANG6
    // eLang7 = 0x96,            // Keyboard LANG7
    // eLang8 = 0x97,            // Keyboard LANG8
    // eLang9 = 0x98,            // Keyboard LANG9
    // eAlternateErase = 0x99,   // Keyboard Alternate Erase
    // eSysReqAttention = 0x9a,  // Keyboard SysReq/Attention
    // eCancel = 0x9b,           // Keyboard Cancel
    // eClear = 0x9c,            // Keyboard Clear
    // ePrior = 0x9d,            // Keyboard Prior
    // eReturn = 0x9e,           // Keyboard Return
    // eSeparator = 0x9f,        // Keyboard Separator
    // eOut = 0xa0,              // Keyboard Out
    // eOper = 0xa1,             // Keyboard Oper
    // eClearAgain = 0xa2,       // Keyboard Clear/Again
    // eCrSelProps = 0xa3,       // Keyboard CrSel/Props
    // eExSel = 0xa4,            // Keyboard ExSel

    // eKeypad00 = 0xb0,         // Keypad 00
    // eKeypad000 = 0xb1,        // Keypad 000
    // eThousandsSeparator = 0xb2, // Thousands Separator
    // eDecimalSeparator = 0xb3, // Decimal Separator
    // eCurrencyUnit = 0xb4,     // Currency Unit
    // eCurrencySubUnit = 0xb5,  // Currency Sub-unit
    eKeypadLeftParen  = 0xb6, // Keypad (
    eKeypadRightParen = 0xb7, // Keypad )
    // eKeypadLeftBrace = 0xb8,  // Keypad {
    // eKeypadRightBrace = 0xb9, // Keypad }
    // eKeypadTab = 0xba,        // Keypad Tab
    // eKeypadBackspace = 0xbb,  // Keypad Backspace
    // eKeypadA = 0xbc,          // Keypad A
    // eKeypadB = 0xbd,          // Keypad B
    // eKeypadC = 0xbe,          // Keypad C
    // eKeypadD = 0xbf,          // Keypad D
    // eKeypadE = 0xc0,          // Keypad E
    // eKeypadF = 0xc1,          // Keypad F
    // eKeypadXor = 0xc2,        // Keypad XOR
    // eKeypadCaret = 0xc3,      // Keypad ^
    // eKeypadPercent = 0xc4,    // Keypad %
    // eKeypadLess = 0xc5,       // Keypad <
    // eKeypadGreater = 0xc6,    // Keypad >
    // eKeypadAmpersand = 0xc7,  // Keypad &
    // eKeypadDoubleAmp = 0xc8,  // Keypad &&
    // eKeypadPipe = 0xc9,       // Keypad |
    // eKeypadDoublePipe = 0xca, // Keypad ||
    // eKeypadColon = 0xcb,      // Keypad :
    // eKeypadHash = 0xcc,       // Keypad #
    // eKeypadSpace = 0xcd,      // Keypad Space
    // eKeypadAt = 0xce,         // Keypad @
    // eKeypadExclaim = 0xcf,    // Keypad !
    // eKeypadMemStore = 0xd0,   // Keypad Memory Store
    // eKeypadMemRecall = 0xd1,  // Keypad Memory Recall
    // eKeypadMemClear = 0xd2,   // Keypad Memory Clear
    // eKeypadMemAdd = 0xd3,     // Keypad Memory Add
    // eKeypadMemSub = 0xd4,     // Keypad Memory Subtract
    // eKeypadMemMult = 0xd5,    // Keypad Memory Multiply
    // eKeypadMemDiv = 0xd6,     // Keypad Memory Divide
    // eKeypadPlusMinus = 0xd7,  // Keypad +/-
    // eKeypadClear = 0xd8,      // Keypad Clear
    // eKeypadClearEntry = 0xd9, // Keypad Clear Entry
    // eKeypadBinary = 0xda,     // Keypad Binary
    // eKeypadOctal = 0xdb,      // Keypad Octal
    // eKeypadDecimal = 0xdc,    // Keypad Decimal
    // eKeypadHexadecimal = 0xdd,// Keypad Hexadecimal

    eLeftCtrl   = 0xe0, // Keyboard Left Control
    eLeftShift  = 0xe1, // Keyboard Left Shift
    eLeftAlt    = 0xe2, // Keyboard Left Alt
    eLeftMeta   = 0xe3, // Keyboard Left GUI
    eRightCtrl  = 0xe4, // Keyboard Right Control
    eRightShift = 0xe5, // Keyboard Right Shift
    eRightAlt   = 0xe6, // Keyboard Right Alt
    eRightMeta  = 0xe7, // Keyboard Right GUI

    eMediaPlayPause    = 0xe8,
    eMediaStopCd       = 0xe9,
    eMediaPreviousSong = 0xea,
    eMediaNextSong     = 0xeb,
    eMediaEjectCd      = 0xec,
    eMediaVolumeUp     = 0xed,
    eMediaVolumeDown   = 0xee,
    eMediaMute         = 0xef,
    eMediaWww          = 0xf0,
    eMediaBack         = 0xf1,
    eMediaForward      = 0xf2,
    eMediaStop         = 0xf3,
    eMediaFind         = 0xf4,
    eMediaScrollUp     = 0xf5,
    eMediaScrollDown   = 0xf6,
    eMediaEdit         = 0xf7,
    eMediaSleep        = 0xf8,
    eMediaCoffee       = 0xf9,
    eMediaRefresh      = 0xfa,
    eMediaCalc         = 0xfb
};

// Operators for the KeyModfier
constexpr auto operator|(KeyModifier lhs, KeyModifier rhs) -> KeyModifier
{
    return static_cast<KeyModifier>(static_cast<uint16_t>(lhs) | static_cast<uint16_t>(rhs));
}
constexpr auto operator&(KeyModifier lhs, KeyModifier rhs) -> KeyModifier
{
    return static_cast<KeyModifier>(static_cast<uint16_t>(lhs) & static_cast<uint16_t>(rhs));
}
constexpr auto operator~(KeyModifier lhs) -> KeyModifier
{
    return static_cast<KeyModifier>(~static_cast<uint16_t>(lhs));
}
constexpr auto operator|=(KeyModifier &lhs, KeyModifier rhs) -> KeyModifier &
{
    lhs = lhs | rhs;
    return lhs;
}
constexpr auto operator&=(KeyModifier &lhs, KeyModifier rhs) -> KeyModifier &
{
    lhs = lhs & rhs;
    return lhs;
}

// operators for the KeyLockState
constexpr auto operator|(KeyLock lhs, KeyLock rhs) -> KeyLock
{
    return static_cast<KeyLock>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}
constexpr auto operator&(KeyLock lhs, KeyLock rhs) -> KeyLock
{
    return static_cast<KeyLock>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
}
constexpr auto operator~(KeyLock lhs) -> KeyLock
{
    return static_cast<KeyLock>(~static_cast<uint8_t>(lhs));
}
constexpr auto operator|=(KeyLock &lhs, KeyLock rhs) -> KeyLock &
{
    lhs = lhs | rhs;
    return lhs;
}
constexpr auto operator&=(KeyLock &lhs, KeyLock rhs) -> KeyLock &
{
    lhs = lhs & rhs;
    return lhs;
}

MKS_PROTO_END
MKS_END