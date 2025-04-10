/**
 * @file communication.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-02-26
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once
#include <mksync/base/base_library.h>

#include <nekoproto/communication/communication_base.hpp>

MKS_BEGIN

// 公共通信接口
class MKS_BASE_API ICommunication {
public:
    ICommunication()          = default;
    virtual ~ICommunication() = default;
    /**
     * @brief 订阅一个消息
     * @note 主要用于服务端节点订阅希望发送给当前客户端的消息
     * 客户端订阅后可以发送消息给服务端。
     * 请不要订阅没有序列化支持的控制协议。
     * @param type 消息类型
     */
    virtual auto subscribes(int type) -> void = 0;
    /**
     * @brief 订阅多个消息
     * @param types 消息类型数组
     */
    virtual auto subscribes(std::vector<int> types) -> void = 0;
    /**
     * @brief 取消订阅一个消息
     * @note 随意取消通信节点的订阅可能导致异常，请谨慎操作！
     * @param type
     */
    virtual auto unsubscribes(int type) -> void = 0;
    /**
     * @brief 取消订阅多个消息
     * @param types
     */
    virtual auto unsubscribes(std::vector<int> types) -> void = 0;
};

/**
 * @brief 服务端通信接口
 * @note 先启动服务端模式才能获取到该接口
 */
class MKS_BASE_API IServerCommunication : public ICommunication {
public:
    IServerCommunication()          = default;
    virtual ~IServerCommunication() = default;
    /**
     * @brief 发送消息给指定节点
     *
     * @param event 消息
     * @param peer 对端（对端来自客户端上线通知消息或peers()）
     * @return ilias::IoTask<void>
     */
    [[nodiscard("coroutine function")]]
    virtual auto send(NekoProto::IProto &event, std::string_view peer) -> ilias::IoTask<void> = 0;

    /**
     * @brief 获取当前在线的所有客户端peer
     *
     * @return std::vector<std::string>
     */
    virtual auto peers() const -> std::vector<std::string> = 0;
};

/**
 * @brief 客户端通信接口
 * @note 先启动客户端模式才能获取到该接口
 */
class MKS_BASE_API IClientCommunication : public ICommunication {
public:
    IClientCommunication()          = default;
    virtual ~IClientCommunication() = default;

    /**
     * @brief 发送消息到服务端
     *
     * @param event 消息
     * @return ilias::IoTask<void>
     */
    [[nodiscard("coroutine function")]]
    virtual auto send(NekoProto::IProto &event) -> ilias::IoTask<void> = 0;
};
MKS_END