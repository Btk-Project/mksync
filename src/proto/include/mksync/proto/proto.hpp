//
// Proto.h
//
// Package: mksync
// Library: MksyncProto
// Module:  Proto
//

#pragma once

#include "mksync/proto/proto_library.h"

#include <stdint.h>
#include <nekoproto/proto/proto_base.hpp>
#include <nekoproto/proto/serializer_base.hpp>
#include <nekoproto/proto/json_serializer.hpp>
#include <nekoproto/proto/types/enum.hpp>

#include "mksync/proto/keycode_converter.hpp"
namespace mks
{
    MKS_PROTO_API uint32_t proto_unused();
    MKS_PROTO_API KeyCode  windows_scan_code_to_key_code(uint32_t scancode, uint32_t vkcode,
                                                         uint16_t *rawcode, bool *virtualKey);
    MKS_PROTO_API uint64_t key_code_to_windows_scan_code(const KeyCode &keyCode);

    enum class MouseButtonState
    {
        eButtonUp   = 0, /**< Mouse button up */
        eButtonDown = 1, /**< Mouse button down */
        eClick      = 2, /**< Mouse button click */
    };

    enum class KeyboardState
    {
        eKeyUp   = 0, /**< Keyboard up */
        eKeyDown = 1, /**< Kerboard down */
    };

    enum class KeyMod
    {
        eKmodNone   = 0X0000U, /**< No Modifier Is Applicable. */
        eKmodLshift = 0X0001U, /**< The Left Shift Key Is Down. */
        eKmodRshift = 0X0002U, /**< The Right Shift Key Is Down. */
        eKmodLevel5 = 0X0004U, /**< The Level 5 Shift Key Is Down. */
        eKmodLctrl  = 0X0040U, /**< The Left Ctrl (Control) Key Is Down. */
        eKmodRctrl  = 0X0080U, /**< The Right Ctrl (Control) Key Is Down. */
        eKmodLalt   = 0X0100U, /**< The Left Alt Key Is Down. */
        eKmodRalt   = 0X0200U, /**< The Right Alt Key Is Down. */
        eKmodLgui   = 0X0400U, /**< The Left Gui Key (Often The Windows Key) Is Down. */
        eKmodRgui   = 0X0800U, /**< The Right Gui Key (Often The Windows Key) Is Down. */
        eKmodNum = 0X1000U, /**< The Num Lock Key (May Be Located On An Extended Keypad) Is Down. */
        eKmodCaps   = 0X2000U,                     /**< The Caps Lock Key Is Down. */
        eKmodMode   = 0X4000U,                     /**< The !Altgr Key Is Down. */
        eKmodScroll = 0X8000U,                     /**< The Scroll Lock Key Is Down. */
        eKmodCtrl   = (eKmodLctrl | eKmodRctrl),   /**< Any Ctrl Key Is Down. */
        eKmodShift  = (eKmodLshift | eKmodRshift), /**< Any Shift Key Is Down. */
        eKmodAlt    = (eKmodLalt | eKmodRalt),     /**< Any Alt Key Is Down. */
        eKmodGui    = (eKmodLgui | eKmodRgui),     /**< Any Gui Key Is Down. */
    };

    enum class MouseButton
    {
        eButtonLeft   = 1, /**< Mouse left button */
        eButtonMiddle = 2, /**< Mouse middle button */
        eButtonRight  = 3, /**< Mouse right button */
        eButtonX1     = 4, /**< Mouse buttonx 1 */
        eButtonX2     = 5, /**< Mouse buttonx 2 */
    };

    struct MouseButtonEvent {
        MouseButtonState state;     /**< Mouse button state */
        MouseButton      button;    /**< Which button */
        uint8_t          clicks;    /**< Clicked a few times */
        uint64_t         timestamp; /**< std::system::system_clock */

        NEKO_SERIALIZER(state, button, clicks, timestamp)
        NEKO_DECLARE_PROTOCOL(MouseButtonEvent, NEKO_NAMESPACE::JsonSerializer)
    };

    struct MouseMotionEvent {
        float    x;         /**< X coordinate, relative to screen */
        float    y;         /**< Y coordinate, relative to screen */
        uint64_t timestamp; /**< std::system::system_clock */

        NEKO_SERIALIZER(x, y, timestamp)
        NEKO_DECLARE_PROTOCOL(MouseMotionEvent, NEKO_NAMESPACE::JsonSerializer)
    };

    struct MouseWheelEvent {
        float x; /**< The amount scrolled horizontally, positive to the right and negative to the
                    left, by default, only this value */
        float y; /**< The amount scrolled vertically, positive away from the user and negative
                    toward the user */
        uint64_t timestamp; /**< std::system::system_clock */

        NEKO_SERIALIZER(x, y, timestamp)
        NEKO_DECLARE_PROTOCOL(MouseWheelEvent, NEKO_NAMESPACE::JsonSerializer)
    };

    struct KeyEvent {
        KeyboardState state;     /**< Keyboard state (up or down) */
        KeyCode       key;       /**< Mks KeyCode enum */
        KeyMod        mod;       /**< Current key modifiers */
        uint64_t      timestamp; /**< std::system::system_clock */

        NEKO_SERIALIZER(state, key, mod, timestamp)
        NEKO_DECLARE_PROTOCOL(KeyEvent, NEKO_NAMESPACE::JsonSerializer)
    };

    struct VirtualScreenInfo {
        std::string name;      /**< Device name reported by the client */
        uint32_t    screenId;  /**< Screen id by this device */
        uint32_t    width;     /**< This screen width */
        uint32_t    height;    /**< This screen height */
        uint64_t    timestamp; /**< std::system::system_clock */

        NEKO_SERIALIZER(name, screenId, width, height, height)
        NEKO_DECLARE_PROTOCOL(VirtualScreenInfo, NEKO_NAMESPACE::JsonSerializer)
    };

} // namespace mks
