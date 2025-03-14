//
// client_proto.h
// protos make by client
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

namespace mks
{
    /**
     * @brief Mouse motion event
     * 服务端收集鼠标事件，并转换成对应的逻辑屏幕上的坐标。
     */
    struct MouseMotionEventConversion {
        float    x;         /**< X coordinate, relative to screen */
        float    y;         /**< Y coordinate, relative to screen */
        uint64_t timestamp; /**< system event time */

        NEKO_SERIALIZER(x, y, timestamp)
        NEKO_DECLARE_PROTOCOL(MouseMotionEventConversion, NEKO_NAMESPACE::JsonSerializer)
    };
} // namespace mks