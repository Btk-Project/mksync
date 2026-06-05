#include "transport.hpp"
#include <ilias/io.hpp>

#include "message.hpp"

MKS_BEGIN

THIS_ERROR_IMPL(RpcError);

RpcTransport::RpcTransport(ilias::DynStream stream) : mStream(std::move(stream)) {
    
}

// TODO: Use serialization library
auto RpcTransport::writeMessage(const RpcMessage &message) -> IoTask<void> {
    ILIAS_CO_TRYV(co_await std::visit([&](const auto &wr) -> IoTask<void> {
        using value_type = std::remove_cvref_t<decltype(wr)>;
        if constexpr (::NekoProto::detail::member_count_v<value_type> == 0) {
            ILIAS_CO_TRYV(co_await writeHeader(0, value_type::messageId()));
        } else {
            std::vector<char> buffer;
            {
                Serializer serializer(buffer);
                if (!serializer(wr)) {
                    SPDLOG_ERROR("RpcTransport::writeMessage: Failed to serialize message");
                    co_return Err(RpcError::UnknownMessageType);
                }
            }
            ILIAS_CO_TRYV(co_await writeHeader(buffer.size(), value_type::messageId()));
            ILIAS_CO_TRYV(co_await mStream.writeAll({
                reinterpret_cast<const std::byte*>(buffer.data()),
                buffer.size()
            }));
        }
        co_return {};
    }, message));
    // SPDLOG_ERROR("RpcTransport::writeMessage: Unknown message type");
    // co_return Err(RpcError::UnknownMessageType);
    ILIAS_CO_TRYV(co_await mStream.flush());
    co_return {};
}

template <typename Variant, typename Func, size_t... Is>
auto findByIdImpl(MessageId targetId, Func&& func, std::index_sequence<Is...>) -> IoResult<Variant> {
    IoResult<Variant> result = Err(RpcError::UnknownMessageType);
    if constexpr (sizeof...(Is) == 0) {
        return Err(RpcError::UnknownMessageType);
    } else {
        ((std::variant_alternative_t<Is, typename Variant::Base>::messageId() == targetId ? (result = std::invoke([&]()-> IoResult<Variant> {
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
    std::vector<char> buffer(size);
    ILIAS_CO_TRYV(co_await mStream.readAll({reinterpret_cast<std::byte*>(buffer.data()), buffer.size()}));
    auto parser = [&]<typename T>() -> IoResult<T> {
        T result;
         if constexpr (::NekoProto::detail::member_count_v<T> > 0) {
            Deserializer deserializer(buffer.data(), buffer.size());
            if (!deserializer(result)) {
                SPDLOG_ERROR("RpcTransport::readMessage: Failed to deserialize HelloMessage");
                return Err(RpcError::UnknownMessageType);
            }
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
