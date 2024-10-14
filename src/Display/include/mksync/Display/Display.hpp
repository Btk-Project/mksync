//
// Display.h
//
// Package: mksync
// Library: MksyncDisplay
// Module:  Display
//

#pragma once

#include "DisplayLibrary.h"

#include <stdint.h>

#include "mksync/Base/osdep.h"

MKS_BEGIN
MKS_DISPLAY_BEGIN

typedef struct DisplayInfo {
    uint32_t    type;
    const void *pNext;
    uint32_t    x;
    uint32_t    y;
    uint32_t    width;
    uint32_t    height;
} MKS_ALIGN8 DisplayInfo;

MKS_DISPLAY_API uint32_t display_unused();

MKS_DISPLAY_END
MKS_END