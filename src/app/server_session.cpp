#include "server_session.hpp"

#include <utility>

MKS_BEGIN

ServerSession::ServerSession(Context context, TcpStream stream, IPEndpoint endpoint)
    : mContext(std::move(context)),
      mTransport(std::move(stream)),
      mEndpoint(endpoint) {
}

auto ServerSession::run() -> IoTask<void> {
    SPDLOG_INFO("Server accepted incoming connection from {}", mEndpoint);

    // Always detach this peer from topology/routing when the session ends —
    // handshake failure, protocol error, cancel, or clean stop.
    struct Guard {
        ServerSession *self;

        ~Guard() {
            SPDLOG_INFO("Server closing connection from {}", self->mEndpoint);
            if (self->mContext.onClosed) {
                self->mContext.onClosed(self->mEndpoint);
            }
        }
    } guard {this};

    ILIAS_CO_TRYV(co_await handshake());

    auto [readResult, writeResult] = co_await ilias::finally(
        ilias::whenAny(readLoop(), writeLoop()),
        shutdown()
    );

    if (readResult && !*readResult) {
        SPDLOG_WARN(
            "Server client read loop ended unexpectedly endpoint={} owner={} name={}: {}",
            mEndpoint,
            mOwnerId,
            mName,
            readResult->error().message()
        );
        co_return Err(readResult->error());
    }
    if (writeResult && !*writeResult) {
        SPDLOG_WARN(
            "Server client write loop ended unexpectedly endpoint={} owner={} name={}: {}",
            mEndpoint,
            mOwnerId,
            mName,
            writeResult->error().message()
        );
        co_return Err(writeResult->error());
    }
    co_return {};
}

auto ServerSession::handshake() -> IoTask<void> {
    ILIAS_CO_TRY(auto msg, co_await mTransport.readMessage());
    auto hello = std::get_if<HelloMessage>(&msg);
    if (!hello) {
        SPDLOG_ERROR("Server received invalid message from {}", mEndpoint);
        co_return Err(RpcError::ProtocolError);
    }

    // machineId is what lets a remote screen keep the same layout even after
    // reconnecting from a different port/address.
    mMachineId = hello->machineId;
    mOwnerId = mMachineId.empty()
        ? mContext.screens.ownerIdFromEndpoint(mEndpoint)
        : mMachineId;
    mContext.screens.rememberOwner(mEndpoint, mOwnerId);
    mName = hello->name;

    if (!isClientTrusted(*hello)) {
        SPDLOG_WARN(
            "Server rejected untrusted client {} name={}",
            mEndpoint,
            hello->name
        );
        co_return Err(RpcError::ProtocolError);
    }

    SPDLOG_INFO(
        "New client connected {}, version: {}, name: {}",
        mEndpoint,
        hello->version,
        hello->name
    );
    co_return {};
}

auto ServerSession::readLoop() -> IoTask<void> {
    while (true) {
        ILIAS_CO_TRY(auto msg, co_await mTransport.readMessage());
        if (auto screens = std::get_if<ScreensMessage>(&msg)) {
            SPDLOG_TRACE(
                "Server received screens endpoint={} owner={} count={}",
                mEndpoint,
                mOwnerId,
                screens->screens.size()
            );
            if (mContext.onScreens) {
                mContext.onScreens(mEndpoint, mOwnerId, screens->screens);
            }
            continue;
        }
        SPDLOG_TRACE("Server received message from {}: {}", mEndpoint, msg);
    }
    co_return {};
}

auto ServerSession::writeLoop() -> IoTask<void> {
    // Publish the sender so ServerInputRouter can enqueue InputMessage without
    // owning this writer coroutine. Channel depth is intentionally small for
    // now; backpressure policy is still open (see docs M8).
    auto [sender, reader] = ilias::mpsc::channel<RpcMessage>(10);
    mSender = sender;
    mContext.senders[mEndpoint] = sender;

    while (true) {
        auto msg = (co_await reader.recv()).value();
        SPDLOG_TRACE("Server writing message to {}: {}", mEndpoint, msg);
        ILIAS_CO_TRYV(co_await mTransport.writeMessage(std::move(msg)));
    }
    co_return {};
}

auto ServerSession::shutdown() -> Task<void> {
    SPDLOG_INFO(
        "Server shutting down client connection endpoint={} owner={} name={}",
        mEndpoint,
        mOwnerId,
        mName
    );
    auto result = co_await mTransport.shutdown();
    if (!result) {
        SPDLOG_WARN(
            "Server transport shutdown failed endpoint={} owner={} name={}: {}",
            mEndpoint,
            mOwnerId,
            mName,
            result.error().message()
        );
    }
    mTransport.close();
    SPDLOG_INFO(
        "Server client connection closed endpoint={} owner={} name={}",
        mEndpoint,
        mOwnerId,
        mName
    );
    co_return;
}

auto ServerSession::isClientTrusted(const HelloMessage &hello) const -> bool {
    return mks::isTrustedClient(
        mContext.screens.config(),
        hello.machineId,
        hello.name
    );
}

MKS_END
