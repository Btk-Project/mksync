#pragma once

#include "preinclude.hpp"
#include "core.hpp"
#include "rpc/message.hpp"
#include "rpc/transport.hpp"
#include "server_input.hpp"
#include "server_screens.hpp"
#include <functional>
#include <ilias/net.hpp>
#include <ilias/task.hpp>
#include <string>
#include <string_view>
#include <vector>

MKS_BEGIN

using ilias::IPEndpoint;
using ilias::TcpStream;

/**
 * @brief One accepted client TCP connection.
 *
 * Owns the RPC transport and the read/write coroutines for a single peer.
 * Shared server state (screens, input senders, layout policy) is referenced
 * through @ref Context — the session never owns topology or capture.
 *
 * Lifecycle:
 * 1. Host accepts a @c TcpStream and resolves @c endpoint.
 * 2. Construct the session and call @c run().
 * 3. Hello handshake, then concurrent read/write until failure or cancel.
 * 4. On exit (any path), @c Context::onClosed removes this endpoint from
 *    routing/topology. Persisted config layout is intentionally kept.
 */
class ServerSession {
public:
    /**
     * @brief Host-owned state and callbacks required by a session.
     *
     * All references must outlive @c run().
     */
    struct Context {
        ServerScreenStore &screens;
        ServerInputRouter::ClientSenders &senders;

        /**
         * @brief Replace remote screens after a @c ScreensMessage.
         *
         * Implemented by Server so store + input active-screen invariants stay
         * consistent (clear active pointer before erasing map nodes).
         */
        std::function<void(
            IPEndpoint endpoint,
            std::string_view ownerId,
            const std::vector<ScreenInfo> &screens
        )> onScreens;

        /**
         * @brief Cleanup after the session ends (always, including failed handshake).
         *
         * Typical work: drop endpoint screens, erase sender, forget owner id.
         */
        std::function<void(IPEndpoint endpoint)> onClosed;
    };

    /**
     * @param context Host references and callbacks.
     * @param stream  Connected client socket (moved into the RPC transport).
     * @param endpoint Peer address already resolved from @p stream (RpcTransport
     *                 does not expose remoteEndpoint on the buffered stream).
     */
    ServerSession(Context context, TcpStream stream, IPEndpoint endpoint);
    ServerSession(const ServerSession &) = delete;
    ServerSession(ServerSession &&) = delete;
    auto operator=(const ServerSession &) -> ServerSession & = delete;
    auto operator=(ServerSession &&) -> ServerSession & = delete;
    ~ServerSession() = default;

    /**
     * @brief Handshake then run reader/writer until failure or cancellation.
     */
    auto run() -> IoTask<void>;

private:
    auto handshake() -> IoTask<void>;
    auto readLoop() -> IoTask<void>;
    auto writeLoop() -> IoTask<void>;
    auto shutdown() -> Task<void>;
    auto isClientTrusted(const HelloMessage &hello) const -> bool;

    Context mContext;
    RpcTransport mTransport;
    IPEndpoint mEndpoint;
    // From HelloMessage; empty when the peer does not send machineId yet.
    std::string mMachineId;
    // Stable owner id used for topology/config (machineId or endpoint string).
    std::string mOwnerId;
    std::string mName;
    // Outbound queue handle mirrored into Context::senders for input routing.
    ilias::mpsc::Sender<RpcMessage> mSender;
};

MKS_END
