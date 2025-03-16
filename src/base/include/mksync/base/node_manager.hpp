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
#include <ilias/ilias.hpp>
#include <ilias/net/tcp.hpp>
#include <ilias/fs/console.hpp>
#include <fmt/format.h>
#include <cxxopts.hpp>
#include <ilias/sync/scope.hpp>

#include "mksync/base/base_library.h"
#include "mksync/base/nodebase.hpp"

namespace mks::base
{
    class MKS_BASE_API NodeDll;
    class MKS_BASE_API IApp;

    class MKS_BASE_API NodeManager {
    public:
        enum NodeStatus
        {
            eNodeStatusRunning = 1,
            eNodeStatusStopped = 2,
        };
        struct NodeData {
            std::unique_ptr<NodeBase, void (*)(NodeBase *)> node;
            NodeStatus                                      status;
        };

    public:
        NodeManager(IApp *app);
        ~NodeManager();

        auto load_node(std::string_view dll) -> std::string_view;
        auto load_nodes(std::string_view path) -> std::vector<std::string_view>;

        auto add_node(std::unique_ptr<NodeBase, void (*)(NodeBase *)> &&node) -> void;

        [[nodiscard("coroutine function")]]
        auto dispatch(const NekoProto::IProto &proto, NodeBase *nodebase) -> ::ilias::Task<void>;
        [[nodiscard("coroutine function")]]
        auto producer_loop(Producer *producer) -> ::ilias::Task<void>;

        auto subscribe(int type, Consumer *consumer) -> void;
        auto subscribe(std::vector<int> types, Consumer *consumer) -> void;
        auto unsubscribe(int type, Consumer *consumer) -> void;
        auto unsubscribe(std::vector<int> types, Consumer *consumer) -> void;

        [[nodiscard("coroutine function")]]
        auto start_node() -> ilias::Task<int>;
        [[nodiscard("coroutine function")]]
        auto start_node(NodeData &node) -> ilias::Task<int>;
        [[nodiscard("coroutine function")]]
        auto start_node(std::string_view name) -> ilias::Task<int>;
        [[nodiscard("coroutine function")]]
        auto stop_node() -> ilias::Task<int>;
        [[nodiscard("coroutine function")]]
        auto stop_node(NodeData &node) -> ilias::Task<int>;
        [[nodiscard("coroutine function")]]
        auto stop_node(std::string_view name) -> ilias::Task<int>;

        auto get_nodes() -> std::vector<NodeData> &;
        auto get_nodes() const -> const std::vector<NodeData> &;
        auto get_node(std::string_view name) -> NodeBase *;

    private:
        IApp                                                                *_app;
        std::vector<NodeData>                                                _nodeList;
        std::unordered_map<int, std::set<Consumer *>>                        _consumerMap;
        ::ilias::TaskScope                                                   _taskScope;
        std::vector<std::unique_ptr<NodeDll>>                                _dlls;
        std::unordered_map<std::string_view, ilias::TaskScope::WaitHandle<>> _cancelHandleMap;
    };
} // namespace mks::base