//
// config_proto.h
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
     * @brief Virtual screen configuration
     * 
     */
    struct VirtualScreenConfig {
        std::string name;
        int32_t     width;
        int32_t     height;
        std::string left;
        std::string top;
        std::string right;
        std::string bottom;

        NEKO_SERIALIZER(name, width, height, left, top, right, bottom)
        NEKO_DECLARE_PROTOCOL(VirtualScreenConfig, NEKO_NAMESPACE::JsonSerializer)
    };
} // namespace mks