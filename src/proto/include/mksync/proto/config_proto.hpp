//
// config_proto.h
// protos make by client
// Package: mksync
// Library: MksyncProto
// Module:  Proto
//

#pragma once
#include <mksync/proto/proto_library.h>

#include <cstdint>

#include <nekoproto/proto/proto_base.hpp>
#include <nekoproto/proto/serializer_base.hpp>
#include <nekoproto/proto/json_serializer.hpp>

MKS_BEGIN

/**
 * @brief Virtual screen configuration
 *
 */
struct VirtualScreenConfig {
    std::string name;
    int32_t     posX;
    int32_t     posY;
    int32_t     width;
    int32_t     height;

    NEKO_SERIALIZER(name, posX, posY, width, height)
    NEKO_DECLARE_PROTOCOL(VirtualScreenConfig, NEKO_NAMESPACE::JsonSerializer)
};

MKS_END