#include "mksync/proto/proto.hpp"

#include <spdlog/spdlog.h>
#ifdef _WIN32
    #include <windows.h>
#else
    #include <xcb/xcb.h>
    #include <xcb/xcb_keysyms.h>
    #include <X11/keysym.h>
    #include <unordered_map>
#endif

namespace mks
{
#ifdef _WIN32
    #define SCANCODE_MAP(WinVk, KeyCode, linuxCode) KeyCode
    const static KeyCode g_keycodeTable[] =
    #include "mksync/proto/win_keycode_table.inc"
    #undef SCANCODE_MAP
        KeyCode windows_scan_code_to_key_code(uint32_t scancode, uint32_t vkcode, uint16_t *rawcode,
                                              bool *virtualKey)
    {
        KeyCode  code;
        uint8_t  index;
        uint16_t keyFlags = HIWORD(scancode);
        uint16_t scanCode = LOBYTE(scancode);

        /* On-Screen Keyboard can send wrong scan codes with high-order bit set (key break code).
         * Strip high-order bit. */
        scanCode &= ~0x80;

        *virtualKey = (scanCode == 0);

        if (scanCode != 0) {
            if ((keyFlags & KF_EXTENDED) == KF_EXTENDED) {
                scanCode = MAKEWORD(scanCode, 0xe0);
            }
            else if (scanCode == 0x45) {
                // Pause
                scanCode = 0xe046;
            }
        }
        else {
            uint16_t vkCode = LOWORD(vkcode);

            /* Windows may not report scan codes for some buttons (multimedia buttons etc).
             * Get scan code from the VK code.*/
            scanCode = LOWORD(MapVirtualKey(vkCode, MAPVK_VK_TO_VSC_EX));

            /* Pause/Break key have a special scan code with 0xe1 prefix.
             * Use Pause scan code that is used in Win32. */
            if (scanCode == 0xe11d) {
                scanCode = 0xe046;
            }
        }

        // Pack scan code into one byte to make the index.
        index    = LOBYTE(scanCode) | (HIBYTE(scanCode) ? 0x80 : 0x00);
        code     = g_keycodeTable[index];
        *rawcode = scanCode;

        return code;
    }

    uint64_t key_code_to_windows_scan_code(const KeyCode &keyCode)
    {
        uint16_t scanCode = 0;

        if (keyCode != KeyCode::eUnknown) {
            for (int i = 0; i < (int)(sizeof(g_keycodeTable) / sizeof(g_keycodeTable[0])); i++) {
                if (g_keycodeTable[i] == keyCode) {
                    scanCode = i;
                    break;
                }
            }
        }
        if (scanCode != 0) {
            if ((scanCode & 0x80) > 0) {
                scanCode |= 0xe000;
            }
        }
        return scanCode;
    }
#elif defined(__linux__)
    #define SCANCODE_MAP(WinVk, KeyCode, linuxCode) {linuxCode, KeyCode}
    const static std::unordered_map<uint32_t, KeyCode>     g_keycodeTable =
    #include "mksync/proto/win_keycode_table.inc"
    #undef SCANCODE_MAP
    #define SCANCODE_MAP(WinVk, KeyCode, linuxCode) {KeyCode, linuxCode}
        const static std::unordered_map<KeyCode, uint32_t> g_keycodeTableReverse =
    #include "mksync/proto/win_keycode_table.inc"
    #undef SCANCODE_MAP
            /**
             * @brief Convert linux keycode or keysym to keycode
             *
             * @param keysyms nullptr or xcb_key_symbols_t* from xcb, nullptr means use keycode
             * @param keysym  keysym from xcb, 0 means use keycode
             * @param keycode  keycode from xcb, 0 means use keysym
             * @return KeyCode
             */
        KeyCode linux_keysym_to_key_code(uint32_t keysym)
    {
        auto item = g_keycodeTable.find(keysym);
        return (item != g_keycodeTable.end()) ? item->second : KeyCode::eUnknown;
    }

    uint32_t key_code_to_linux_keysym(KeyCode keyCode)
    {
        auto item = g_keycodeTableReverse.find(keyCode);
        return (item != g_keycodeTableReverse.end()) ? item->second : XK_VoidSymbol;
    }

#endif
} // namespace mks