#include "mksync/proto/keycode_converter.hpp"

#include <spdlog/spdlog.h>

namespace mks
{
#ifdef _WIN32
    KeyCode windows_scan_code_to_key_code(uint32_t scancode, uint32_t vkcode, uint16_t *rawcode,
                                          bool *virtualKey)
    {
        KeyCode  code;
        uint8_t  index;
        uint16_t keyFlags = HIWORD(scancode);
        uint16_t scanCode = LOBYTE(keyFlags);

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
        index = LOBYTE(scanCode) | (HIBYTE(scanCode) ? 0x80 : 0x00);
        spdlog::debug("scancode {}, index {}", scanCode, index);
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

#endif
} // namespace mks