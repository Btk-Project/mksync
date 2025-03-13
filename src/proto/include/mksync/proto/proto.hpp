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
#include "mksync/proto/system_proto.hpp"
#include "mksync/proto/client_proto.hpp"
#include "mksync/proto/command_proto.hpp"
#include "mksync/proto/server_proto.hpp"

namespace mks
{
    MKS_PROTO_API uint32_t proto_unused();

} // namespace mks
