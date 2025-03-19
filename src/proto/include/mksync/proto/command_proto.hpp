//
// command_proto.h
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

namespace mks
{
    // 服务端相关命令
    /**
     * @brief start/stop/restart server
     * 来自用户或控制器的命令，开启或关闭服务端程序。
     */
    struct ServerControl {
        enum Command
        {
            eStart,
            eStop,
            eRestart,
        };

        Command     cmd;  /**< 命令 */
        std::string ip;   /**< 需要监听的ip地址 */
        uint16_t    port; /**< 需要监听的端口号 */

        NEKO_SERIALIZER(cmd, ip, port)
        NEKO_DECLARE_PROTOCOL(ServerControl, NEKO_NAMESPACE::JsonSerializer)
    };

    /**
     * @brief capture module control
     * 根据焦点屏幕由控制器触发，开启/关闭捕获模块的拦截。
     */
    struct CaptureControl {
        enum Command
        {
            eStart,
            eStop,
        };

        Command cmd; /**< 命令 */

        NEKO_SERIALIZER(cmd)
        NEKO_DECLARE_PROTOCOL(CaptureControl, NEKO_NAMESPACE::JsonSerializer)
    };

    /**
     * @brief 焦点屏幕改变
     *
     * @note 该协议为本地控制协议，不参与序列化，请不要通过网络发送。
     */
    struct FocusScreenChanged {
        std::string name;    /**< 获取焦点的屏幕 */
        std::string peer;    /**< 在通信中的对端名称 */
        std::string oldName; /**< 旧的焦点所在屏幕 */
        std::string oldPeer; /**< 旧的焦点所在对端 */

        NEKO_SERIALIZER() // 不需要序列化的协议
        NEKO_DECLARE_PROTOCOL(FocusScreenChanged, NEKO_NAMESPACE::JsonSerializer)
    };

    /**
     * @brief 有来自客户端连接。
     * 服务端与来自客户端的握手成功后由通信节点生成该事件，上报连接的客户端消息。
     * @note 该协议为本地控制协议，不参与序列化，请不要通过网络发送。
     */
    struct ClientConnected {
        std::string       peer; /**< 客户端名称 */
        VirtualScreenInfo info; /**< 客户端屏幕信息 */

        NEKO_SERIALIZER() // 不需要序列化的协议
        NEKO_DECLARE_PROTOCOL(ClientConnected, NEKO_NAMESPACE::JsonSerializer)
    };

    /**
     * @brief 有来自客户端消息。
     *
     * @note 该协议为本地控制协议，不参与序列化，请不要通过网络发送。
     */
    struct ClientMessage {
        std::string                        peer;  /**< 客户端名称 */
        std::shared_ptr<NekoProto::IProto> proto; /**< 消息 */

        NEKO_SERIALIZER() // 不需要序列化的协议
        NEKO_DECLARE_PROTOCOL(ClientMessage, NEKO_NAMESPACE::JsonSerializer)
    };

    /**
     * @brief 有来自客户端断开连接。
     * 服务端与来自客户端的连接断开后由通信节点生成该事件，上报客户端断开消息。
     * @note 该协议为本地控制协议，不参与序列化，请不要通过网络发送。
     */
    struct ClientDisconnected {
        std::string peer;   /**< 客户端名称 */
        std::string reason; /**< 断开原因 */

        NEKO_SERIALIZER() // 不需要序列化的协议
        NEKO_DECLARE_PROTOCOL(ClientDisconnected, NEKO_NAMESPACE::JsonSerializer)
    };

    // 客户端相关命令
    /**
     * @brief start/stop/restart client
     *
     */
    struct ClientControl {
        enum Command
        {
            eStart,
            eStop,
            eRestart,
        };

        Command     cmd;
        std::string ip;
        uint16_t    port;

        NEKO_SERIALIZER(cmd, ip, port)
        NEKO_DECLARE_PROTOCOL(ClientControl, NEKO_NAMESPACE::JsonSerializer)
    };

    /**
     * @brief sender module control
     * 由控制器触发，开启/关闭发送模块的发送。默认情况下作为客户端应该可以全程开着。
     */
    struct SenderControl {
        enum Command
        {
            eStart,
            eStop,
        };

        Command cmd;

        NEKO_SERIALIZER(cmd)
        NEKO_DECLARE_PROTOCOL(SenderControl, NEKO_NAMESPACE::JsonSerializer)
    };

    /**
     * @brief 以服务端/客户端开始/结束
     *
     * @note 该协议为本地控制协议，不参与序列化，请不要通过网络发送。
     */
    struct AppStatusChanged {
        enum Status
        {
            eStarted,
            eStopped,
        };
        enum Mode
        {
            eServer,
            eClient,
        };

        Status status;
        Mode   mode;

        NEKO_SERIALIZER() // 不需要序列化的协议
        NEKO_DECLARE_PROTOCOL(AppStatusChanged, NEKO_NAMESPACE::JsonSerializer)
    };

} // namespace mks