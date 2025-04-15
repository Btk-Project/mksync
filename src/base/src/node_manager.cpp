#include "mksync/base/node_manager.hpp"

#ifdef _WIN32
    #include <Windows.h>
    #define DL_OPEN(dll) LoadLibraryW(dll)
    #define DL_CLOSE(handle) FreeLibrary(handle)
    #define DL_LOAD_SYMBOL(handle, symbol) GetProcAddress(handle, symbol)
    #define DL_HANDLE HMODULE
#elif defined(__linux__)
    #include <dlfcn.h>
    #define DL_OPEN(dll) dlopen(dll, RTLD_LAZY)
    #define DL_CLOSE(handle) dlclose(handle)
    #define DL_LOAD_SYMBOL(handle, symbol) dlsym(handle, symbol)
    #define DL_HANDLE void *
#endif

#include <filesystem>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/callback_sink.h>

#include "mksync/base/nodebase.hpp"
#include "mksync/base/app.hpp"

MKS_BEGIN

using ::ilias::IoTask;
using ::ilias::IPEndpoint;
using ::ilias::Task;

class MKS_BASE_API NodeDll {
public:
    NodeDll() {}
    auto load(std::string_view dll) -> bool
    {
        // TODO: check dll string encoding
        if (!std::filesystem::exists(dll)) {
            SPDLOG_ERROR("load plugins failed! file {} not exists!", dll);
            return false;
        }
        std::filesystem::path dllPath(dll);
#ifdef _WIN32
        _handle = DL_OPEN(dllPath.wstring().c_str());
#elif defined(__linux__)
        _handle = DL_OPEN(dllPath.string().c_str());
#endif
        return _handle != nullptr;
    }
    auto create_node(IApp *app) -> std::unique_ptr<NodeBase, void (*)(NodeBase *)>
    {
        if (_handle == nullptr) {
            return {nullptr, +[](NodeBase *) {}};
        }
        auto *createNode = reinterpret_cast<MKS_MAKE_NODE_FUNC_TYPE>(
            DL_LOAD_SYMBOL(_handle, MKS_MAKE_NODE_FUNC_NAME));
        auto deleteNode = reinterpret_cast<MKS_DELETE_NODE_FUNC_TYPE>(
            DL_LOAD_SYMBOL(_handle, MKS_DELETE_NODE_FUNC_NAME));
        if (createNode == nullptr || deleteNode == nullptr) {
            SPDLOG_ERROR("load plugins failed! createNode or deleteNode is nullptr!");
            return {nullptr, +[](NodeBase *) {}};
        }
        return {(NodeBase *)createNode(app), deleteNode};
    }
    ~NodeDll()
    {
        if (_handle != nullptr) {
            DL_CLOSE(_handle);
        }
    }

private:
    DL_HANDLE _handle = nullptr;
};

    NodeManager::NodeManager(IApp *app) : _app(app), _taskScope(*app->get_io_context())
    {
        _taskScope.spawn(_events_loop());
    }

NodeManager::~NodeManager()
{
    teardown_node().wait();
    _taskScope.cancel();
}

auto NodeManager::load_node(std::string_view dll) -> std::string_view
{
    _dlls.emplace_back(std::make_unique<NodeDll>());
    if (_dlls.back()->load(dll)) {
        auto node = _dlls.back()->create_node(_app);
        if (node == nullptr) {
            SPDLOG_ERROR("load plugin from {} failed!", dll);
            _dlls.pop_back();
            return "";
        }
        if (auto item = _nodeMap.find(node->name()); item != _nodeMap.end()) {
            SPDLOG_ERROR("load plugin from {} failed! node({}) already exists!", dll, node->name());
            _dlls.pop_back();
            return "";
        }
        return add_node(std::move(node));
    }
    _dlls.pop_back();
    return "";
}

auto NodeManager::load_nodes(std::string_view path) -> std::vector<std::string_view>
{
    std::vector<std::string_view> names;
    for (const auto &file : std::filesystem::directory_iterator(path)) {
        if (file.is_regular_file()) {
            auto name = load_node(file.path().string());
            if (!name.empty()) {
                names.push_back(name);
            }
        }
    }
    return names;
}

auto NodeManager::add_node(std::unique_ptr<NodeBase, void (*)(NodeBase *)> &&node)
    -> std::string_view
{
    if (node == nullptr) {
        return "";
    }
    const auto *name = node->name();
    if (auto item = _nodeMap.find(name); item != _nodeMap.end()) {
        if (item->second->status == eNodeDestroyed) {
            item->second->node   = std::move(node);
            item->second->status = eNodeStatusStopped;
            return name;
        }
        SPDLOG_ERROR("add node: {}<{}> failed! node already exists!", name, (void *)node.get());
        return name;
    }
    SPDLOG_INFO("add node: {}<{}>", name, (void *)node.get());
    _nodeMap.insert(std::make_pair(
        name, _nodeList.emplace(_nodeList.end(),
                                NodeData{std::move(node), NodeStatus::eNodeStatusStopped})));
    return name;
}

    auto NodeManager::destroy_node(std::string_view name) -> int
    {
        auto item = _nodeMap.find(name);
        if (item == _nodeMap.end()) {
            return -2;
        }
        if (item->second->status == eNodeStatusRunning) {
            SPDLOG_ERROR("destroy node: {}<{}> failed! node is running!");
            return -1;
        }
        if (_isInProcess) {
            item->second->status = eNodeDestroyed;
        }
        else {
            _isInProcess = true;
            SPDLOG_INFO("destroy node: {}<{}>", item->second->node->name(),
                        (void *)item->second->node.get());
            _nodeList.erase(item->second);
            _nodeMap.erase(item);
            _isInProcess = false;
        }
        // _nodeList.erase(item->second);
        // _nodeMap.erase(item);
        return 0;
    }

auto NodeManager::setup_node(std::string_view name) -> ilias::Task<int>
{
    if (auto item = _nodeMap.find(name); item != _nodeMap.end()) {
        co_return co_await setup_node(*(item->second));
    }
    co_return -1;
}

    auto NodeManager::setup_node(NodeData &node) -> ilias::Task<int>
    {
        SPDLOG_INFO("start node: {}<{}>", node.node->name(), (void *)node.node.get());
        if (node.status == NodeStatus::eNodeStatusStopped) {
            auto ret = co_await node.node->setup();
            if (ret != 0) {
                co_return ret;
            }
            node.status    = NodeStatus::eNodeStatusRunning;
            auto *consumer = dynamic_cast<Consumer *>(node.node.get());
            if (consumer != nullptr) {
                for (int type : consumer->get_subscribes()) {
                    subscribe(type, consumer);
                }
            }
            auto *producer = dynamic_cast<Producer *>(node.node.get());
            if (producer != nullptr) {
                _cancelHandleMap[node.node->name()] = _taskScope.spawn(_producer_loop(producer));
            }
            co_return 0;
        }
        co_return -2;
    }

    auto NodeManager::_producer_loop(Producer *producer) -> ::ilias::Task<void>
    {
        while (true) {
            if (auto ret = co_await producer->get_event(); ret) {
                if (ret.value() == nullptr) {
                    SPDLOG_CRITICAL("producer: {}<{}> get event a null proto",
                                    dynamic_cast<NodeBase *>(producer)->name(),
                                    (void *)dynamic_cast<NodeBase *>(producer));
                    break;
                }
                co_await dispatch(ret.value(), dynamic_cast<NodeBase *>(producer));
            }
            else if (ret.error() != ::ilias::Error::Canceled) {
                SPDLOG_ERROR("producer: {}<{}> get event failed! exit producer loop!, {}",
                             dynamic_cast<NodeBase *>(producer)->name(), (void *)producer,
                             ret.has_value() ? "null proto" : ret.error().message());
                break;
            }
            else {
                SPDLOG_INFO("producer: {}<{}> get event canceled! exit producer loop!",
                            dynamic_cast<NodeBase *>(producer)->name(),
                            (void *)dynamic_cast<NodeBase *>(producer));
                break;
            }
        }
        co_return;
    }

    auto NodeManager::_events_loop() -> ::ilias::Task<void>
    {
        while (true) {
            EventBase::Event event;
            if (auto ret = co_await _events.pop(event); ret == ::ilias::Error::Ok) {
                co_await dispatch(event.data, event.from);
            }
            else if (ret != ::ilias::Error::Canceled) {
                SPDLOG_ERROR("get event failed! exit events loop!, {}", ret.message());
                break;
            }
            else {
                SPDLOG_INFO("get event canceled! exit events loop!");
                break;
            }
        }
    }

auto NodeManager::subscribe(int type, Consumer *consumer) -> void
{
    SPDLOG_INFO("subscribe: {}<{}> type: {}", dynamic_cast<NodeBase *>(consumer)->name(),
                (void *)dynamic_cast<NodeBase *>(consumer), type);
    _consumerMap[type].insert(consumer);
}

auto NodeManager::subscribe(std::vector<int> types, Consumer *consumer) -> void
{
    for (int type : types) {
        SPDLOG_INFO("subscribe: {}<{}> type: {}", dynamic_cast<NodeBase *>(consumer)->name(),
                    (void *)dynamic_cast<NodeBase *>(consumer), type);
        _consumerMap[type].insert(consumer);
    }
}

auto NodeManager::unsubscribe(int type, Consumer *consumer) -> void
{
    SPDLOG_INFO("unsubscribe: {}<{}> type: {}", dynamic_cast<NodeBase *>(consumer)->name(),
                (void *)dynamic_cast<NodeBase *>(consumer), type);
    _consumerMap[type].erase(consumer);
}

auto NodeManager::unsubscribe(std::vector<int> types, Consumer *consumer) -> void
{
    for (int type : types) {
        SPDLOG_INFO("unsubscribe: {}<{}> type: {}", dynamic_cast<NodeBase *>(consumer)->name(),
                    (void *)dynamic_cast<NodeBase *>(consumer), type);
        _consumerMap[type].erase(consumer);
    }
}

auto NodeManager::teardown_node(std::string_view name) -> ilias::Task<int>
{
    if (auto item = _nodeMap.find(name); item != _nodeMap.end()) {
        co_return co_await teardown_node(*(item->second));
    }
    co_return -1;
}

auto NodeManager::teardown_node(NodeData &node) -> ilias::Task<int>
{
    if (node.status == NodeStatus::eNodeStatusRunning) {
        node.status    = NodeStatus::eNodeStatusStopped;
        auto *consumer = dynamic_cast<Consumer *>(node.node.get());
        if (consumer != nullptr) {
            for (auto item = _consumerMap.begin(); item != _consumerMap.end();) {
                // 遍历所有协议的消费者表，删除防止其运行时订阅的遗漏。
                SPDLOG_INFO("unsubscribe: {}<{}> type: {}",
                            dynamic_cast<NodeBase *>(consumer)->name(),
                            (void *)dynamic_cast<NodeBase *>(consumer), item->first);
                item->second.erase(consumer);
                if (item->second.size() == 0) {
                    item = _consumerMap.erase(item);
                }
                else {
                    ++item;
                }
            }
        }
        auto *producer = dynamic_cast<Producer *>(node.node.get());
        if (producer != nullptr) {
            auto handle = _cancelHandleMap.find(node.node->name());
            if (handle != _cancelHandleMap.end() && handle->second) {
                handle->second.cancel();
                co_await std::move(handle->second);
            }
            _cancelHandleMap.erase(handle);
        }
        co_return co_await node.node->teardown();
    }
    co_return 0;
}

auto NodeManager::dispatch(const NekoProto::IProto &proto, NodeBase *nodebase) -> Task<void>
{
    int type = proto.type();
    for (auto *consumer : _consumerMap[type]) {
        if (proto == nullptr) {
            break;
        }
        if (dynamic_cast<NodeBase *>(consumer) == nodebase) {
            continue;
        }
        // SPDLOG_INFO("dispatch {} from {} to {}", proto.protoName(),
        //             nodebase == nullptr ? "unknow" : nodebase->name(),
        //             dynamic_cast<NodeBase *>(consumer)->name());
        co_await consumer->handle_event(proto);
    }
    co_return;
}

auto NodeManager::setup_node() -> ilias::Task<int>
{
    auto ret = 0;
    for (auto &node : _nodeList) {
        if (auto re = co_await setup_node(node); re != 0) {
            ret = re;
            SPDLOG_ERROR("node {} start failed!", node.node->name());
        }
    }
    co_return ret;
}

auto NodeManager::teardown_node() -> ilias::Task<int>
{
    auto ret     = 0;
    _isInProcess = true;
    for (auto node = _nodeList.rbegin(); node != _nodeList.rend(); ++node) {
        // FIXME:过程中删除节点在windows上可以运行，在ubuntu上会出现内存异常
        if (auto re = co_await teardown_node(*node); re != 0) {
            ret = re;
            SPDLOG_ERROR("node {} stop failed!", node->node->name());
        }
    }
    for (auto item = _nodeMap.begin(); item != _nodeMap.end();) {
        if (item->second->status == eNodeDestroyed) {
            _nodeList.erase(item->second);
            item = _nodeMap.erase(item);
        }
        else {
            ++item;
        }
    }
    _isInProcess = false;
    co_return ret;
}

auto NodeManager::get_nodes() -> std::list<NodeData> &
{
    return _nodeList;
}

    auto NodeManager::get_nodes() const -> const std::list<NodeData> &
    {
        return _nodeList;
    }

    auto NodeManager::get_node(std::string_view name) -> NodeBase *
    {
        if (auto item = _nodeMap.find(name); item != _nodeMap.end()) {
            return item->second->node.get();
        }
        return nullptr;
    }

auto NodeManager::get_events() -> EventBase &
{
    return _events;
}

MKS_END