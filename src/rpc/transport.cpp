#include "transport.hpp"
#include <ilias/io.hpp>
#include <limits>

#include "message.hpp"

MKS_BEGIN

THIS_ERROR_IMPL(RpcError);

RpcTransport::RpcTransport(ilias::DynStream stream) : mStream(std::move(stream)) {
    
}

// TODO: Use serialization library
auto RpcTransport::writeMessage(const RpcMessage &message) -> IoTask<void> {
    ILIAS_CO_TRYV(co_await std::visit([&](const auto &wr) -> IoTask<void> {
        std::vector<char> buffer;
        {
            Serializer serializer(buffer);
            if (!serializer(wr)) {
                SPDLOG_ERROR("RpcTransport::writeMessage: Failed to serialize message");
                co_return Err(RpcError::UnknownMessageType);
            }
        }
        if (buffer.size() > std::numeric_limits<uint16_t>::max()) {
            SPDLOG_ERROR("RpcTransport::writeMessage: Message too large: {} bytes", buffer.size());
            co_return Err(RpcError::MessageTooLarge);
        }
        ILIAS_CO_TRYV(co_await writeHeader(static_cast<uint16_t>(buffer.size()), wr.Id));
        ILIAS_CO_TRYV(co_await mStream.writeAll(ilias::makeBuffer(buffer)));
        co_return {};
    }, message));
    ILIAS_CO_TRYV(co_await mStream.flush());
    co_return {};
}

template <typename Variant, typename Func, size_t... Is>
auto findByIdImpl(MessageId targetId, Func&& func, std::index_sequence<Is...>) -> IoResult<Variant> {
    IoResult<Variant> result = Err(RpcError::UnknownMessageType);
    if constexpr (sizeof...(Is) == 0) {
        return Err(RpcError::UnknownMessageType);
    }
    else {
        (void)((std::variant_alternative_t<Is, typename Variant::Base>::Id == targetId ? (result = std::invoke([&]()-> IoResult<Variant> {
            auto opt = func.template operator()<std::variant_alternative_t<Is, typename Variant::Base>>();
            if (opt.has_value()) {
                return Variant(std::move(*opt));
            }
            return Err(opt.error());
        }), true) : false) || ...);

        return result;
    }
}

template <typename Variant, typename Func>
auto findById(MessageId targetId, Func&& func) -> IoResult<Variant> {
    return findByIdImpl<Variant>(targetId, std::forward<Func>(func), std::make_index_sequence<std::variant_size_v<typename Variant::Base>>());
}

auto RpcTransport::readMessage() -> IoTask<RpcMessage> {
    ILIAS_CO_TRY(auto header, co_await readHeader());
    auto [size, id] = header;

    // Read payload into buffer
    std::vector<std::byte> buffer;
    buffer.resize(size);
    ILIAS_CO_TRYV(co_await mStream.readAll(buffer));

    auto parser = [&]<typename T>() -> IoResult<T> {
        T result;
        Deserializer deserializer(reinterpret_cast<const char *>(buffer.data()), buffer.size());
        if (!deserializer(result)) {
            SPDLOG_ERROR("RpcTransport::readMessage: Failed to deserialize HelloMessage");
            return Err(RpcError::UnknownMessageType);
        }
        return result;
    };
    co_return findById<RpcMessage>(id, parser);
}

// Header ...
auto RpcTransport::writeHeader(uint16_t size, MessageId id) -> IoTask<void> {
    ILIAS_CO_TRYV(co_await mStream.writeU16Be(size));
    ILIAS_CO_TRYV(co_await mStream.writeU16Be(static_cast<uint16_t>(id)));
    co_return {};
}

auto RpcTransport::readHeader() -> IoTask<std::pair<uint16_t, MessageId> > {
    ILIAS_CO_TRY(auto size, co_await mStream.readU16Be());
    ILIAS_CO_TRY(auto id, co_await mStream.readU16Be());
    co_return std::pair {size, static_cast<MessageId>(id)};
}

MKS_END
