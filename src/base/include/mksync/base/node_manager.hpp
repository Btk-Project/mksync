/**
 * @file node_manager.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-03-13
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include <set>
#include <list>
#include <ilias/ilias.hpp>
#include <ilias/net/tcp.hpp>
#include <ilias/fs/console.hpp>
#include <fmt/format.h>
#include <cxxopts.hpp>
#include <ilias/sync/scope.hpp>

#include "mksync/base/base_library.h"
#include "mksync/base/nodebase.hpp"
#include "mksync/base/event_base.hpp"

namespace mks::base
{
    class MKS_BASE_API NodeDll;
    class MKS_BASE_API IApp;

    /**
     * @brief 节点管理器
     *
     */
    class MKS_BASE_API NodeManager {
    public:
        enum NodeStatus
        {
            eNodeStatusRunning = 1,
            eNodeStatusStopped = 2,
            eNodeDestroyed     = 2,
        };
        struct NodeData {
            std::unique_ptr<NodeBase, void (*)(NodeBase *)> node;
            NodeStatus                                      status;
        };

    public:
        NodeManager(IApp *app);
        ~NodeManager();

        /**
         * @brief 加载节点
         *
         * @param dll dll文件路径
         * @return std::string_view
         */
        auto load_node(std::string_view dll) -> std::string_view;

        /**
         * @brief 加载节点
         *
         * @param path 节点所在路径
         * @return std::vector<std::string_view>
         */
        auto load_nodes(std::string_view path) -> std::vector<std::string_view>;

        /**
         * @brief 加入创建好的节点实例
         *
         * @param node
         * @return std::string_view
         */
        auto add_node(std::unique_ptr<NodeBase, void (*)(NodeBase *)> &&node) -> std::string_view;

        /**
         * @brief 销毁节点
         *
         * @return int
         * 0: 成功
         * -1: 节点正在运行
         * -2: 节点不存在
         */
        auto destroy_node(std::string_view) -> int;

        /**
         * @brief 分发协议
         *
         * @param proto 启动协议
         * @param nodebase 协议的所属节点
         * @return ::ilias::Task<void>
         */
        [[nodiscard("coroutine function")]]
        auto dispatch(const NekoProto::IProto &proto, NodeBase *nodebase) -> ::ilias::Task<void>;

        /**
         * @brief 为消费者订阅一个协议
         *
         * @param type 协议类型
         * @param consumer 消费者的指针
         */
        auto subscribe(int type, Consumer *consumer) -> void;
        auto subscribe(std::vector<int> types, Consumer *consumer) -> void;

        /**
         * @brief 为消费者取消订阅一个协议
         *
         * @param type 协议类型
         * @param consumer 消费者指针
         */
        auto unsubscribe(int type, Consumer *consumer) -> void;
        auto unsubscribe(std::vector<int> types, Consumer *consumer) -> void;

        /**
         * @brief 启动所有节点
         *
         * @return ilias::Task<int>
         */
        [[nodiscard("coroutine function")]]
        auto setup_node() -> ilias::Task<int>;
        /**
         * @brief 启动节点
         *
         * @param node 节点对象引用
         * @return ilias::Task<int>
         */
        [[nodiscard("coroutine function")]]
        auto setup_node(NodeData &node) -> ilias::Task<int>;
        /**
         * @brief 启动节点
         *
         * @param name 节点名称
         * @return ilias::Task<int>
         */
        [[nodiscard("coroutine function")]]
        auto setup_node(std::string_view name) -> ilias::Task<int>;
        /**
         * @brief 停止所有节点
         *
         * @return ilias::Task<int>
         */
        [[nodiscard("coroutine function")]]
        auto teardown_node() -> ilias::Task<int>;
        /**
         * @brief 停止节点
         *
         * @param node 节点引用
         * @return ilias::Task<int>
         */
        [[nodiscard("coroutine function")]]
        auto teardown_node(NodeData &node) -> ilias::Task<int>;
        /**
         * @brief 停止节点
         *
         * @param name 节点名称
         * @return ilias::Task<int>
         */
        [[nodiscard("coroutine function")]]
        auto teardown_node(std::string_view name) -> ilias::Task<int>;

        /**
         * @brief 获取所有节点
         *
         * @return std::list<NodeData>&
         */
        auto get_nodes() -> std::list<NodeData> &;
        auto get_nodes() const -> const std::list<NodeData> &;

        /**
         * @brief 获取节点
         *
         * @param name 节点名称
         * @return NodeBase*
         */
        auto get_node(std::string_view name) -> NodeBase *;

        /**
         * @brief 获取事件容器
         *
         * @return EventBase&
         */
        auto get_events() -> EventBase &;

    private:
        /**
         * @brief 生产者循环
         * @note 所有生产者节点都会生成单独的协程循环
         * @param producer 生产者节点指针
         * @return ::ilias::Task<void>
         */
        [[nodiscard("coroutine function")]]
        auto _producer_loop(Producer *producer) -> ::ilias::Task<void>;

        /**
         * @brief 事件循环
         * @note 节点管理器会自动启动一个事件循环用于分发事件。由于是协程循环，因此不能重复启动。
         * @return ::ilias::Task<void>
         */
        [[nodiscard("coroutine function")]]
        auto _events_loop() -> ::ilias::Task<void>;

    private:
        IApp               *_app;
        EventBase           _events;
        std::list<NodeData> _nodeList;
        ::ilias::TaskScope  _taskScope;
        // 是否正在teardown节点，在teardown过程中不能删除节点
        bool _isInProcess = false;
        // 协议类型 -> 订阅了该协议的消费者集合
        std::map<int, std::set<Consumer *>> _consumerMap;
        // 节点名称 -> 节点迭代器
        std::unordered_map<std::string_view, std::list<NodeData>::iterator> _nodeMap;
        std::vector<std::unique_ptr<NodeDll>>                               _dlls;
        // 节点名称 -> 生产者节点循环的取消句柄
        std::unordered_map<std::string_view, ilias::TaskScope::WaitHandle<>> _cancelHandleMap;
    };
} // namespace mks::base