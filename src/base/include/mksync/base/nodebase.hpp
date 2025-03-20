/**
 * @file nodebase.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-02-25
 * 定义了节点插件的必要接口和需要导出的头。插件的说明：
 * 职责：
 *  必须实现MKS_MAKE_NODE_FUNC和MKS_DELETE_NODE_FUNC
 *      - 加载模块时会调用MKS_MAKE_NODE_FUNC创建节点。
 *          - 该函数的实现必须创建并初始化好节点
 *          -
 * 该函数需要自行注册自己模块需要的命令，如何注册命令请查看类App的文档，所有模块都会自动生成start/stop命令。
 *      - 删除由模块创建的节点会调用MKS_DELETE_NODE_FUNC
 *  必须继承并实现NodeBase, 可以选择是否继承并实现Consumer，Producer接口
 *      - NodeBase
 *          - start() 在启动节点时会调用，协程函数。
 *          - stop() 在停止节点时会调用，协程函数。
 *          - name()
 * 返回节点名称，用于命令行显示，生成start/stop命令时使用。请确保该名称和在mks_make_node函数中注册的命令所属模块一致。
 *      - Consumer
 *          - get_subscribes()
 * 返回该消费者需要订阅的事件列表，当有事件发生时，会调用handle_event。类型id可以通过ProtoFactory::protoType<TYPE>()获取。
 *          - handle_event() 处理一个事件，需要订阅。
 *      - Producer
 *          - get_event() 从节点内获取一个事件，如果没有就等待（需要可以被取消）。
 *          - post_event() 尝试从节点内获取一个事件，如果没有返回错误。
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include <nekoproto/communication/communication_base.hpp>

#include "mksync/base/base_library.h"

namespace mks::base
{
    class IApp;
    /**
     * @brief Node
     * 节点基类，作为插件的最低要求，必须至少实现该类的接口。
     * - name
     * 节点的名字。
     * - start
     * 启动节点，协程函数。不可以阻塞。加载节点后会按顺序启动节点。
     * - stop
     * 停止节点，协程函数。不可以阻塞。卸载节点时会按顺序停止节点。退出软件时会流程。
     */
    class MKS_BASE_API NodeBase {
    public:
        enum Result
        {
            eError   = -1,
            eSuccess = 0,
        };

    public:
        NodeBase()          = default;
        virtual ~NodeBase() = default;
        ///> 启用节点。
        [[nodiscard("coroutine function")]]
        virtual auto setup() -> ::ilias::Task<int> = 0;
        ///> 停用节点。
        [[nodiscard("coroutine function")]]
        virtual auto teardown() -> ::ilias::Task<int> = 0;
        ///> 获取节点名称。
        virtual auto name() -> const char * = 0;
    };

    /**
     * @brief Consumer
     * 消费者接口，用于订阅事件，当事件发生时，会调用handle_event。
     * - get_subscribers
     * 返回该消费者需要订阅的事件列表，当有事件发生时，会调用handle_event。类型id可以通过ProtoFactory::protoType<TYPE>()获取。
     * - handle_event
     * 当有生产者产生订阅的事件时，会调用该函数处理事件。
     */
    class MKS_BASE_API Consumer {
    public:
        Consumer()          = default;
        virtual ~Consumer() = default;
        ///> 订阅的事件集，当有事件发生时，会调用handle_event。需要返回，只在enable时使用。
        /// 如果需要动态增减，可以使用NodeManager的接口。
        virtual auto get_subscribes() -> std::vector<int> = 0;
        ///> 处理一个事件，需要订阅。
        [[nodiscard("coroutine function")]]
        virtual auto handle_event(const NekoProto::IProto &event) -> ::ilias::Task<void> = 0;
    };

    /**
     * @brief producer
     * 生产者接口，可以生产任意事件，需要通过协程co_await。
     * - get_event
     * 从节点内获取一个事件，如果没有就使用协程等待。只有当需要产出大量事件或者想要自己定义独立的事件缓冲区才实现该接口。
     */
    class MKS_BASE_API Producer {
    public:
        Producer()          = default;
        virtual ~Producer() = default;
        ///> 从节点内获取一个事件，如果没有就等待。
        [[nodiscard("coroutine function")]]
        virtual auto get_event() -> ::ilias::IoTask<NekoProto::IProto> = 0;
    };

} // namespace mks::base

#define MKS_MAKE_NODE_FUNC_NAME "mks_make_node"
#define MKS_MAKE_NODE_FUNC                                                                         \
    extern "C" ::mks::base::NodeBase *mks_make_node([[maybe_unused]] void *app)
#define MKS_DELETE_NODE_FUNC_NAME "mks_delete_node"
#define MKS_DELETE_NODE_FUNC extern "C" void mks_delete_node(::mks::base::NodeBase *node)
///> 从dll中创建一个NodeBase对象，如果有命令需要注册可以通过app的接口。
typedef void *(*MKS_MAKE_NODE_FUNC_TYPE)(void *app);
///> 删除该dll创建出来的node对象。
typedef void (*MKS_DELETE_NODE_FUNC_TYPE)(::mks::base::NodeBase *node);