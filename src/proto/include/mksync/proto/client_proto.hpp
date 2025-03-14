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
     * @brief Hello Event
     * 连接成功后客户端主动上报程序名和版本号
     * 收到到HelloEvent后，主动发送VirtualScreenInfo
     *
     */
    struct HelloEvent {
        std::string name;    /**< client name */
        std::string version; /**< server/client version */

        NEKO_SERIALIZER(name, version)
        NEKO_DECLARE_PROTOCOL(HelloEvent, NEKO_NAMESPACE::JsonSerializer)
    };

} // namespace mks
