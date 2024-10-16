//
// layout.h
//
// Package: mksync
// Library: MksyncDisplay
// Module:  Display
//

#pragma once

#include "mksync/display/display_library.h"

#include <stdint.h>
#include <vector>

#include "mksync/base/osdep.h"
#include "mksync/display/display.hpp"

MKS_BEGIN
MKS_DISPLAY_BEGIN

typedef struct LayoutInfo {
    uint32_t                 type;
    const void              *pNext;
    std::vector<DisplayInfo> displayInfos;
} MKS_ALIGN8 LayoutInfo;

MKS_DISPLAY_END
MKS_END