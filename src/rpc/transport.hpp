#pragma once

#include "preinclude.hpp"
#include "refl/this_error.hpp"
#include "message.hpp"
#include <ilias/io.hpp>

MKS_BEGIN

// Wire format:
// u16 size (the message size, excluding the size, type itself)
// u16 type
// u8[...] message

enum class RpcError {
    Ok = 0,
    MessageTooLarge,
    UnknownMessageType,
    ProtocolError,
};
THIS_ERROR(RpcError);

/**
 * @brief The RpcTransport, used to deserialize and serialize RPC messages between the client and server.
 * 
 */
class RpcTransport {
public:
    explicit RpcTransport(ilias::DynStream stream);
    RpcTransport(RpcTransport &&) = default;

    auto writeMessage(const RpcMessage &msg) -> IoTask<void>;
    auto readMessage() -> IoTask<RpcMessage>;
    auto shutdown() -> IoTask<void>;
    auto close() -> void;
private:
    auto writeHeader(uint16_t size, MessageId id) -> IoTask<void>;
    auto readHeader() -> IoTask<std::pair<uint16_t, MessageId> >;

    ilias::BufStream<ilias::DynStream> mStream;
};

MKS_END
