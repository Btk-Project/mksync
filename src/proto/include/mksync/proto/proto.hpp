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

namespace mks
{
    MKS_PROTO_API uint32_t proto_unused();

    enum class KeyCode : uint32_t
    {
        eUnknown            = 0X00000000U, /**< 0 */
        eWakeUp             = 0X00000002U, /**< 2 */
        eReturn             = 0x0000000DU, /**< '\r' */
        eEscape             = 0X0000001BU, /**< esc */
        eBackspace          = 0X00000008U, /**< ' */
        eTab                = 0X00000009U, /**< '   ' */
        eSpace              = 0X00000020U, /**< ' ' */
        eExclaim            = 0X00000021U, /**< '!' */
        eDblapostrophe      = 0X00000022U, /**< '"' */
        eHash               = 0X00000023U, /**< '#' */
        eDollar             = 0X00000024U, /**< '$' */
        ePercent            = 0X00000025U, /**< '%' */
        eAmpersand          = 0X00000026U, /**< '&' */
        eApostrophe         = 0X00000027U, /**< ''' */
        eLeftparen          = 0X00000028U, /**< '(' */
        eRightparen         = 0X00000029U, /**< ')' */
        eAsterisk           = 0X0000002AU, /**< '*' */
        ePlus               = 0X0000002BU, /**< '+' */
        eComma              = 0X0000002CU, /**< ',' */
        eMinus              = 0X0000002DU, /**< '-' */
        ePeriod             = 0X0000002EU, /**< '.' */
        eSlash              = 0X0000002FU, /**< '/' */
        e0                  = 0X00000030U, /**< '0' */
        e1                  = 0X00000031U, /**< '1' */
        e2                  = 0X00000032U, /**< '2' */
        e3                  = 0X00000033U, /**< '3' */
        e4                  = 0X00000034U, /**< '4' */
        e5                  = 0X00000035U, /**< '5' */
        e6                  = 0X00000036U, /**< '6' */
        e7                  = 0X00000037U, /**< '7' */
        e8                  = 0X00000038U, /**< '8' */
        e9                  = 0X00000039U, /**< '9' */
        eColon              = 0X0000003AU, /**< ':' */
        eSemicolon          = 0X0000003BU, /**< ';' */
        eLess               = 0X0000003CU, /**< '<' */
        eEquals             = 0X0000003DU, /**< '=' */
        eGreater            = 0X0000003EU, /**< '>' */
        eQuestion           = 0X0000003FU, /**< '?' */
        eAt                 = 0X00000040U, /**< '@' */
        eLeftbracket        = 0X0000005BU, /**< '[' */
        eBackslash          = 0X0000005CU, /**< '\' */
        eRightbracket       = 0X0000005DU, /**< ']' */
        eCaret              = 0X0000005EU, /**< '^' */
        eUnderScore         = 0X0000005FU, /**< '_' */
        eGrave              = 0X00000060U, /**< '`' */
        eA                  = 0X00000061U, /**< 'A' */
        eB                  = 0X00000062U, /**< 'B' */
        eC                  = 0X00000063U, /**< 'C' */
        eD                  = 0X00000064U, /**< 'D' */
        eE                  = 0X00000065U, /**< 'E' */
        eF                  = 0X00000066U, /**< 'F' */
        eG                  = 0X00000067U, /**< 'G' */
        eH                  = 0X00000068U, /**< 'H' */
        eI                  = 0X00000069U, /**< 'I' */
        eJ                  = 0X0000006AU, /**< 'J' */
        eK                  = 0X0000006BU, /**< 'K' */
        eL                  = 0X0000006CU, /**< 'L' */
        eM                  = 0X0000006DU, /**< 'M' */
        eN                  = 0X0000006EU, /**< 'N' */
        eO                  = 0X0000006FU, /**< 'O' */
        eP                  = 0X00000070U, /**< 'P' */
        eQ                  = 0X00000071U, /**< 'Q' */
        eR                  = 0X00000072U, /**< 'R' */
        eS                  = 0X00000073U, /**< 'S' */
        eT                  = 0X00000074U, /**< 'T' */
        eU                  = 0X00000075U, /**< 'U' */
        eV                  = 0X00000076U, /**< 'V' */
        eW                  = 0X00000077U, /**< 'W' */
        eX                  = 0X00000078U, /**< 'X' */
        eY                  = 0X00000079U, /**< 'Y' */
        eZ                  = 0X0000007AU, /**< 'Z' */
        eLeftbrace          = 0X0000007BU, /**< '{' */
        ePipe               = 0X0000007CU, /**< '|' */
        eRightbrace         = 0X0000007DU, /**< '}' */
        eTilde              = 0X0000007EU, /**< '~' */
        eDelete             = 0X0000007FU, /**< 'del' */
        ePlusminus          = 0X000000B1U, /**< '±' */
        eCapslock           = 0X40000039U, /**< ScancodeToKeycode(ScancodeCapslock) */
        eF1                 = 0X4000003AU, /**< ScancodeToKeycode(ScancodeF1) */
        eF2                 = 0X4000003BU, /**< ScancodeToKeycode(ScancodeF2) */
        eF3                 = 0X4000003CU, /**< ScancodeToKeycode(ScancodeF3) */
        eF4                 = 0X4000003DU, /**< ScancodeToKeycode(ScancodeF4) */
        eF5                 = 0X4000003EU, /**< ScancodeToKeycode(ScancodeF5) */
        eF6                 = 0X4000003FU, /**< ScancodeToKeycode(ScancodeF6) */
        eF7                 = 0X40000040U, /**< ScancodeToKeycode(ScancodeF7) */
        eF8                 = 0X40000041U, /**< ScancodeToKeycode(ScancodeF8) */
        eF9                 = 0X40000042U, /**< ScancodeToKeycode(ScancodeF9) */
        eF10                = 0X40000043U, /**< ScancodeToKeycode(ScancodeF10) */
        eF11                = 0X40000044U, /**< ScancodeToKeycode(ScancodeF11) */
        eF12                = 0X40000045U, /**< ScancodeToKeycode(ScancodeF12) */
        ePrintscreen        = 0X40000046U, /**< ScancodeToKeycode(ScancodePrintscreen) */
        eScrolllock         = 0X40000047U, /**< ScancodeToKeycode(ScancodeScrolllock) */
        ePause              = 0X40000048U, /**< ScancodeToKeycode(ScancodePause) */
        eInsert             = 0X40000049U, /**< ScancodeToKeycode(ScancodeInsert) */
        eHome               = 0X4000004AU, /**< ScancodeToKeycode(ScancodeHome) */
        ePageup             = 0X4000004BU, /**< ScancodeToKeycode(ScancodePageup) */
        eEnd                = 0X4000004DU, /**< ScancodeToKeycode(ScancodeEnd) */
        ePagedown           = 0X4000004EU, /**< ScancodeToKeycode(ScancodePagedown) */
        eRight              = 0X4000004FU, /**< ScancodeToKeycode(ScancodeRight) */
        eLeft               = 0X40000050U, /**< ScancodeToKeycode(ScancodeLeft) */
        eDown               = 0X40000051U, /**< ScancodeToKeycode(ScancodeDown) */
        eUp                 = 0X40000052U, /**< ScancodeToKeycode(ScancodeUp) */
        eNumlockclear       = 0X40000053U, /**< ScancodeToKeycode(ScancodeNumlockclear) */
        eKpDivide           = 0X40000054U, /**< ScancodeToKeycode(ScancodeKpDivide) */
        eKpMultiply         = 0X40000055U, /**< ScancodeToKeycode(ScancodeKpMultiply) */
        eKpMinus            = 0X40000056U, /**< ScancodeToKeycode(ScancodeKpMinus) */
        eKpPlus             = 0X40000057U, /**< ScancodeToKeycode(ScancodeKpPlus) */
        eKpEnter            = 0X40000058U, /**< ScancodeToKeycode(ScancodeKpEnter) */
        eKp1                = 0X40000059U, /**< ScancodeToKeycode(ScancodeKp1) */
        eKp2                = 0X4000005AU, /**< ScancodeToKeycode(ScancodeKp2) */
        eKp3                = 0X4000005BU, /**< ScancodeToKeycode(ScancodeKp3) */
        eKp4                = 0X4000005CU, /**< ScancodeToKeycode(ScancodeKp4) */
        eKp5                = 0X4000005DU, /**< ScancodeToKeycode(ScancodeKp5) */
        eKp6                = 0X4000005EU, /**< ScancodeToKeycode(ScancodeKp6) */
        eKp7                = 0X4000005FU, /**< ScancodeToKeycode(ScancodeKp7) */
        eKp8                = 0X40000060U, /**< ScancodeToKeycode(ScancodeKp8) */
        eKp9                = 0X40000061U, /**< ScancodeToKeycode(ScancodeKp9) */
        eKp0                = 0X40000062U, /**< ScancodeToKeycode(ScancodeKp0) */
        eKpPeriod           = 0X40000063U, /**< ScancodeToKeycode(ScancodeKpPeriod) */
        eApplication        = 0X40000065U, /**< ScancodeToKeycode(ScancodeApplication) */
        ePower              = 0X40000066U, /**< ScancodeToKeycode(ScancodePower) */
        eKpEquals           = 0X40000067U, /**< ScancodeToKeycode(ScancodeKpEquals) */
        eF13                = 0X40000068U, /**< ScancodeToKeycode(ScancodeF13) */
        eF14                = 0X40000069U, /**< ScancodeToKeycode(ScancodeF14) */
        eF15                = 0X4000006AU, /**< ScancodeToKeycode(ScancodeF15) */
        eF16                = 0X4000006BU, /**< ScancodeToKeycode(ScancodeF16) */
        eF17                = 0X4000006CU, /**< ScancodeToKeycode(ScancodeF17) */
        eF18                = 0X4000006DU, /**< ScancodeToKeycode(ScancodeF18) */
        eF19                = 0X4000006EU, /**< ScancodeToKeycode(ScancodeF19) */
        eF20                = 0X4000006FU, /**< ScancodeToKeycode(ScancodeF20) */
        eF21                = 0X40000070U, /**< ScancodeToKeycode(ScancodeF21) */
        eF22                = 0X40000071U, /**< ScancodeToKeycode(ScancodeF22) */
        eF23                = 0X40000072U, /**< ScancodeToKeycode(ScancodeF23) */
        eF24                = 0X40000073U, /**< ScancodeToKeycode(ScancodeF24) */
        eExecute            = 0X40000074U, /**< ScancodeToKeycode(ScancodeExecute) */
        eHelp               = 0X40000075U, /**< ScancodeToKeycode(ScancodeHelp) */
        eMenu               = 0X40000076U, /**< ScancodeToKeycode(ScancodeMenu) */
        eSelect             = 0X40000077U, /**< ScancodeToKeycode(ScancodeSelect) */
        eStop               = 0X40000078U, /**< ScancodeToKeycode(ScancodeStop) */
        eAgain              = 0X40000079U, /**< ScancodeToKeycode(ScancodeAgain) */
        eUndo               = 0X4000007AU, /**< ScancodeToKeycode(ScancodeUndo) */
        eCut                = 0X4000007BU, /**< ScancodeToKeycode(ScancodeCut) */
        eCopy               = 0X4000007CU, /**< ScancodeToKeycode(ScancodeCopy) */
        ePaste              = 0X4000007DU, /**< ScancodeToKeycode(ScancodePaste) */
        eFind               = 0X4000007EU, /**< ScancodeToKeycode(ScancodeFind) */
        eMute               = 0X4000007FU, /**< ScancodeToKeycode(ScancodeMute) */
        eVolumeup           = 0X40000080U, /**< ScancodeToKeycode(ScancodeVolumeup) */
        eVolumedown         = 0X40000081U, /**< ScancodeToKeycode(ScancodeVolumedown) */
        eKpComma            = 0X40000085U, /**< ScancodeToKeycode(ScancodeKpComma) */
        eKpEqualsas400      = 0X40000086U, /**< ScancodeToKeycode(ScancodeKpEqualsas400) */
        eAlterase           = 0X40000099U, /**< ScancodeToKeycode(ScancodeAlterase) */
        eSysreq             = 0X4000009AU, /**< ScancodeToKeycode(ScancodeSysreq) */
        eCancel             = 0X4000009BU, /**< ScancodeToKeycode(ScancodeCancel) */
        eClear              = 0X4000009CU, /**< ScancodeToKeycode(ScancodeClear) */
        ePrior              = 0X4000009DU, /**< ScancodeToKeycode(ScancodePrior) */
        eReturn2            = 0X4000009EU, /**< ScancodeToKeycode(ScancodeReturn2) */
        eSeparator          = 0X4000009FU, /**< ScancodeToKeycode(ScancodeSeparator) */
        eOut                = 0X400000A0U, /**< ScancodeToKeycode(ScancodeOut) */
        eOper               = 0X400000A1U, /**< ScancodeToKeycode(ScancodeOper) */
        eClearagain         = 0X400000A2U, /**< ScancodeToKeycode(ScancodeClearagain) */
        eCrsel              = 0X400000A3U, /**< ScancodeToKeycode(ScancodeCrsel) */
        eExsel              = 0X400000A4U, /**< ScancodeToKeycode(ScancodeExsel) */
        eKp00               = 0X400000B0U, /**< ScancodeToKeycode(ScancodeKp00) */
        eKp000              = 0X400000B1U, /**< ScancodeToKeycode(ScancodeKp000) */
        eThousandsseparator = 0X400000B2U, /**< ScancodeToKeycode(ScancodeThousandsseparator) */
        eDecimalseparator   = 0X400000B3U, /**< ScancodeToKeycode(ScancodeDecimalseparator) */
        eCurrencyunit       = 0X400000B4U, /**< ScancodeToKeycode(ScancodeCurrencyunit) */
        eCurrencysubunit    = 0X400000B5U, /**< ScancodeToKeycode(ScancodeCurrencysubunit) */
        eKpLeftparen        = 0X400000B6U, /**< ScancodeToKeycode(ScancodeKpLeftparen) */
        eKpRightparen       = 0X400000B7U, /**< ScancodeToKeycode(ScancodeKpRightparen) */
        eKpLeftbrace        = 0X400000B8U, /**< ScancodeToKeycode(ScancodeKpLeftbrace) */
        eKpRightbrace       = 0X400000B9U, /**< ScancodeToKeycode(ScancodeKpRightbrace) */
        eKpTab              = 0X400000BaU, /**< ScancodeToKeycode(ScancodeKpTab) */
        eKpBackspace        = 0X400000BbU, /**< ScancodeToKeycode(ScancodeKpBackspace) */
        eKpA                = 0X400000BcU, /**< ScancodeToKeycode(ScancodeKpA) */
        eKpB                = 0X400000BdU, /**< ScancodeToKeycode(ScancodeKpB) */
        eKpC                = 0X400000BeU, /**< ScancodeToKeycode(ScancodeKpC) */
        eKpD                = 0X400000BfU, /**< ScancodeToKeycode(ScancodeKpD) */
        eKpE                = 0X400000C0U, /**< ScancodeToKeycode(ScancodeKpE) */
        eKpF                = 0X400000C1U, /**< ScancodeToKeycode(ScancodeKpF) */
        eKpXor              = 0X400000C2U, /**< ScancodeToKeycode(ScancodeKpXor) */
        eKpPower            = 0X400000C3U, /**< ScancodeToKeycode(ScancodeKpPower) */
        eKpPercent          = 0X400000C4U, /**< ScancodeToKeycode(ScancodeKpPercent) */
        eKpLess             = 0X400000C5U, /**< ScancodeToKeycode(ScancodeKpLess) */
        eKpGreater          = 0X400000C6U, /**< ScancodeToKeycode(ScancodeKpGreater) */
        eKpAmpersand        = 0X400000C7U, /**< ScancodeToKeycode(ScancodeKpAmpersand) */
        eKpDblampersand     = 0X400000C8U, /**< ScancodeToKeycode(ScancodeKpDblampersand) */
        eKpVerticalbar      = 0X400000C9U, /**< ScancodeToKeycode(ScancodeKpVerticalbar) */
        eKpDblverticalbar   = 0X400000CaU, /**< ScancodeToKeycode(ScancodeKpDblverticalbar) */
        eKpColon            = 0X400000CbU, /**< ScancodeToKeycode(ScancodeKpColon) */
        eKpHash             = 0X400000CcU, /**< ScancodeToKeycode(ScancodeKpHash) */
        eKpSpace            = 0X400000CdU, /**< ScancodeToKeycode(ScancodeKpSpace) */
        eKpAt               = 0X400000CeU, /**< ScancodeToKeycode(ScancodeKpAt) */
        eKpExclam           = 0X400000CfU, /**< ScancodeToKeycode(ScancodeKpExclam) */
        eKpMemstore         = 0X400000D0U, /**< ScancodeToKeycode(ScancodeKpMemstore) */
        eKpMemrecall        = 0X400000D1U, /**< ScancodeToKeycode(ScancodeKpMemrecall) */
        eKpMemclear         = 0X400000D2U, /**< ScancodeToKeycode(ScancodeKpMemclear) */
        eKpMemadd           = 0X400000D3U, /**< ScancodeToKeycode(ScancodeKpMemadd) */
        eKpMemsubtract      = 0X400000D4U, /**< ScancodeToKeycode(ScancodeKpMemsubtract) */
        eKpMemmultiply      = 0X400000D5U, /**< ScancodeToKeycode(ScancodeKpMemmultiply) */
        eKpMemdivide        = 0X400000D6U, /**< ScancodeToKeycode(ScancodeKpMemdivide) */
        eKpPlusminus        = 0X400000D7U, /**< ScancodeToKeycode(ScancodeKpPlusminus) */
        eKpClear            = 0X400000D8U, /**< ScancodeToKeycode(ScancodeKpClear) */
        eKpClearentry       = 0X400000D9U, /**< ScancodeToKeycode(ScancodeKpClearentry) */
        eKpBinary           = 0X400000DaU, /**< ScancodeToKeycode(ScancodeKpBinary) */
        eKpOctal            = 0X400000DbU, /**< ScancodeToKeycode(ScancodeKpOctal) */
        eKpDecimal          = 0X400000DcU, /**< ScancodeToKeycode(ScancodeKpDecimal) */
        eKpHexadecimal      = 0X400000DdU, /**< ScancodeToKeycode(ScancodeKpHexadecimal) */
        eLctrl              = 0X400000E0U, /**< ScancodeToKeycode(ScancodeLctrl) */
        eLshift             = 0X400000E1U, /**< ScancodeToKeycode(ScancodeLshift) */
        eLalt               = 0X400000E2U, /**< ScancodeToKeycode(ScancodeLalt) */
        eLgui               = 0X400000E3U, /**< ScancodeToKeycode(ScancodeLgui) */
        eRctrl              = 0X400000E4U, /**< ScancodeToKeycode(ScancodeRctrl) */
        eRshift             = 0X400000E5U, /**< ScancodeToKeycode(ScancodeRshift) */
        eRalt               = 0X400000E6U, /**< ScancodeToKeycode(ScancodeRalt) */
        eRgui               = 0X400000E7U, /**< ScancodeToKeycode(ScancodeRgui) */
        eMode               = 0X40000101U, /**< ScancodeToKeycode(ScancodeMode) */
        eSleep              = 0X40000102U, /**< ScancodeToKeycode(ScancodeSleep) */
        eWake               = 0X40000103U, /**< ScancodeToKeycode(ScancodeWake) */
        eChannelIncrement   = 0X40000104U, /**< ScancodeToKeycode(ScancodeChannelIncrement) */
        eChannelDecrement   = 0X40000105U, /**< ScancodeToKeycode(ScancodeChannelDecrement) */
        eMediaPlay          = 0X40000106U, /**< ScancodeToKeycode(ScancodeMediaPlay) */
        eMediaPause         = 0X40000107U, /**< ScancodeToKeycode(ScancodeMediaPause) */
        eMediaRecord        = 0X40000108U, /**< ScancodeToKeycode(ScancodeMediaRecord) */
        eMediaFastForward   = 0X40000109U, /**< ScancodeToKeycode(ScancodeMediaFastForward) */
        eMediaRewind        = 0X4000010AU, /**< ScancodeToKeycode(ScancodeMediaRewind) */
        eMediaNextTrack     = 0X4000010BU, /**< ScancodeToKeycode(ScancodeMediaNextTrack) */
        eMediaPreviousTrack = 0X4000010CU, /**< ScancodeToKeycode(ScancodeMediaPreviousTrack) */
        eMediaStop          = 0X4000010DU, /**< ScancodeToKeycode(ScancodeMediaStop) */
        eMediaEject         = 0X4000010EU, /**< ScancodeToKeycode(ScancodeMediaEject) */
        eMediaPlayPause     = 0X4000010FU, /**< ScancodeToKeycode(ScancodeMediaPlayPause) */
        eMediaSelect        = 0X40000110U, /**< ScancodeToKeycode(ScancodeMediaSelect) */
        eAcNew              = 0X40000111U, /**< ScancodeToKeycode(ScancodeAcNew) */
        eAcOpen             = 0X40000112U, /**< ScancodeToKeycode(ScancodeAcOpen) */
        eAcClose            = 0X40000113U, /**< ScancodeToKeycode(ScancodeAcClose) */
        eAcExit             = 0X40000114U, /**< ScancodeToKeycode(ScancodeAcExit) */
        eAcSave             = 0X40000115U, /**< ScancodeToKeycode(ScancodeAcSave) */
        eAcPrint            = 0X40000116U, /**< ScancodeToKeycode(ScancodeAcPrint) */
        eAcProperties       = 0X40000117U, /**< ScancodeToKeycode(ScancodeAcProperties) */
        eAcSearch           = 0X40000118U, /**< ScancodeToKeycode(ScancodeAcSearch) */
        eAcHome             = 0X40000119U, /**< ScancodeToKeycode(ScancodeAcHome) */
        eAcBack             = 0X4000011AU, /**< ScancodeToKeycode(ScancodeAcBack) */
        eAcForward          = 0X4000011BU, /**< ScancodeToKeycode(ScancodeAcForward) */
        eAcStop             = 0X4000011CU, /**< ScancodeToKeycode(ScancodeAcStop) */
        eAcRefresh          = 0X4000011DU, /**< ScancodeToKeycode(ScancodeAcRefresh) */
        eAcBookmarks        = 0X4000011EU, /**< ScancodeToKeycode(ScancodeAcBookmarks) */
        eSoftleft           = 0X4000011FU, /**< ScancodeToKeycode(ScancodeSoftleft) */
        eSoftright          = 0X40000120U, /**< ScancodeToKeycode(ScancodeSoftright) */
        eCall               = 0X40000121U, /**< ScancodeToKeycode(ScancodeCall) */
        eEndcall            = 0X40000122U, /**< ScancodeToKeycode(ScancodeEndcall) */
        eLeftTab            = 0X20000001U, /**< Extended Key Left Tab */
        eLevel5Shift        = 0X20000002U, /**< Extended Key Level 5 Shift */
        eMultiKeyCompose    = 0X20000003U, /**< Extended Key Multi-Key Compose */
        eLmeta              = 0X20000004U, /**< Extended Key Left Meta */
        eRmeta              = 0X20000005U, /**< Extended Key Right Meta */
        eLhyper             = 0X20000006U, /**< Extended Key Left Hyper */
        eRhyper             = 0X20000007U, /**< Extended Key Right Hyper */

        eExtendedMask = 1U << 29,
        eScancodeMask = 1U << 30,
    };

#ifdef _WIN32
    MKS_PROTO_API KeyCode  windows_scan_code_to_key_code(uint32_t scancode, uint32_t vkcode,
                                                         uint16_t *rawcode, bool *virtualKey);
    MKS_PROTO_API uint64_t key_code_to_windows_scan_code(const KeyCode &keyCode);
#elif defined(__linux__)
    /**
     * @brief Convert linux keycode or keysym to keycode
     *
     * @param keysyms nullptr or xcb_key_symbols_t* from xcb, nullptr means use keycode
     * @param keysym  keysym from xcb, 0 means use keycode
     * @param keycode  keycode from xcb, 0 means use keysym
     * @return KeyCode
     */
    MKS_PROTO_API KeyCode  linux_keysym_to_key_code(uint32_t keysym);
    MKS_PROTO_API uint32_t key_code_to_linux_keysym(KeyCode keyCode);
#endif
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

    enum class KeyMod : uint16_t
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
        uint64_t         timestamp; /**< system event time */

        NEKO_SERIALIZER(state, button, clicks, timestamp)
        NEKO_DECLARE_PROTOCOL(MouseButtonEvent, NEKO_NAMESPACE::JsonSerializer)
    };

    struct MouseMotionEvent {
        float    x;          /**< X coordinate, relative to screen */
        float    y;          /**< Y coordinate, relative to screen */
        bool     isAbsolute; /**< is the point is absolute */
        uint64_t timestamp;  /**< system event time */

        NEKO_SERIALIZER(x, y, isAbsolute, timestamp)
        NEKO_DECLARE_PROTOCOL(MouseMotionEvent, NEKO_NAMESPACE::JsonSerializer)
    };

    struct MouseWheelEvent {
        float x; /**< The amount scrolled horizontally, positive to the right and negative to the
                    left, by default, only this value */
        float y; /**< The amount scrolled vertically, positive away from the user and negative
                    toward the user */
        uint64_t timestamp; /**< system event time */

        NEKO_SERIALIZER(x, y, timestamp)
        NEKO_DECLARE_PROTOCOL(MouseWheelEvent, NEKO_NAMESPACE::JsonSerializer)
    };

    struct KeyEvent {
        KeyboardState state;     /**< Keyboard state (up or down) */
        KeyCode       key;       /**< Mks KeyCode enum */
        KeyMod        mod;       /**< Current key modifiers */
        uint64_t      timestamp; /**< system event time */

        NEKO_SERIALIZER(state, key, mod, timestamp)
        NEKO_DECLARE_PROTOCOL(KeyEvent, NEKO_NAMESPACE::JsonSerializer)
    };

    struct VirtualScreenInfo {
        std::string name;      /**< Device name reported by the client */
        uint32_t    screenId;  /**< Screen id by this device */
        uint32_t    width;     /**< This screen width */
        uint32_t    height;    /**< This screen height */
        uint64_t    timestamp; /**< system event time */

        NEKO_SERIALIZER(name, screenId, width, height, height)
        NEKO_DECLARE_PROTOCOL(VirtualScreenInfo, NEKO_NAMESPACE::JsonSerializer)
    };

    /**
     * @brief Hello Event
     * 首次通信由客户端发起。客户端上报版本号与屏幕数目，服务端匹配版本好并分配屏幕Id。
     * 分配Id完成后，客户端通过VirtualScreenInfo上报屏幕信息。
     * id:
     *  server -> client:
     *      服务器将分配一个屏幕Id给到客户端
     */
    struct HelloEvent {
        std::string name;    /**< client name */
        int         version; /**< server/client version */

        NEKO_SERIALIZER(name, version)
        NEKO_DECLARE_PROTOCOL(HelloEvent, NEKO_NAMESPACE::JsonSerializer)
    };

    struct BorderEvent {
        enum Border
        {
            eLeft   = 1 << 0,
            eRight  = 1 << 1,
            eTop    = 1 << 2,
            eBottom = 1 << 3
        };
        uint32_t screenId; /**< screen id */
        uint32_t border;   /**< border */
        float    x;        /**< X coordinate, relative to screen */
        float    y;        /**< Y coordinate, relative to screen */

        NEKO_SERIALIZER(screenId, border, x, y)
        NEKO_DECLARE_PROTOCOL(BorderEvent, NEKO_NAMESPACE::JsonSerializer)
    };

} // namespace mks
