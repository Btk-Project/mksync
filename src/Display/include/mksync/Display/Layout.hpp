//
// Layout.h
//
// Package: mksync
// Library: MksyncDisplay
// Module:  Display
//

#pragma once

#include "DisplayLibrary.h"

#include <stdint.h>
#include <vector>

#include "mksync/Base/osdep.h"
#include "mksync/Display/Display.hpp"

MKS_BEGIN
MKS_DISPLAY_BEGIN

typedef struct LayoutInfo {
    uint32_t                 type;
    const void              *pNext;
    std::vector<DisplayInfo> displayInfos;
} MKS_ALIGN8 LayoutInfo;

MKS_DISPLAY_END
MKS_END