#include "mksync/base/node_manager.hpp"

#include <spdlog/sinks/callback_sink.h>
#include <spdlog/spdlog.h>
#include <filesystem>
#ifdef _WIN32
    #include <windows.h>
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

#include "mksync/base/nodebase.hpp"
#include "mksync/base/app.hpp"

namespace mks::base
{
    using ::ilias::IoTask;
    using ::ilias::IPEndpoint;
    using ::ilias::Task;

    class MKS_BASE_API NodeDll {
    public:
        NodeDll() {}
        auto load(std::string_view dll) -> bool
        {
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

    NodeManager::NodeManager(IApp *app) : _app(app), _taskScope(*app->get_io_context()) {}

    NodeManager::~NodeManager()
    {
        stop_node().wait();
        _taskScope.cancel();
    }

    auto NodeManager::load_node(std::string_view dll) -> std::string_view
    {
        _dlls.emplace_back(std::make_unique<NodeDll>());
        if (_dlls.back()->load(dll)) {
            auto node = _dlls.back()->create_node(_app);
            if (node == nullptr) {
                SPDLOG_ERROR("load plugin from {} failed!", dll);
                return "";
            }
            const auto *name = node->name();
            add_node(std::move(node));
            return name;
        }
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

    auto NodeManager::add_node(std::unique_ptr<NodeBase, void (*)(NodeBase *)> &&node) -> void
    {
        if (node == nullptr) {
            return;
        }
        const auto *name = node->name();
        SPDLOG_INFO("install node: {}<{}>", name, (void *)node.get());
        _nodeList.emplace_back(NodeData{std::move(node), NodeStatus::eNodeStatusStopped});
    }

    auto NodeManager::start_node(std::string_view name) -> ilias::Task<int>
    {
        for (auto &node : _nodeList) {
            if (node.node->name() == name) {
                co_return co_await start_node(node);
            }
        }
        co_return -1;
    }

    auto NodeManager::start_node(NodeData &node) -> ilias::Task<int>
    {
        SPDLOG_INFO("start node: {}<{}>", node.node->name(), (void *)node.node.get());
        if (node.status == NodeStatus::eNodeStatusStopped) {
            auto ret = co_await node.node->enable();
            if (ret != 0) {
                co_return ret;
            }
            node.status    = NodeStatus::eNodeStatusRunning;
            auto *consumer = dynamic_cast<Consumer *>(node.node.get());
            if (consumer != nullptr) {
                for (int type : consumer->get_subscribes()) {
                    _consumerMap[type].insert(consumer);
                }
            }
            auto *producer = dynamic_cast<Producer *>(node.node.get());
            if (producer != nullptr) {
                _cancelHandleMap[node.node->name()] = _taskScope.spawn(producer_loop(producer));
            }
            co_return 0;
        }
        co_return -2;
    }

    auto NodeManager::producer_loop(Producer *producer) -> ::ilias::Task<void>
    {
        while (true) {
            if (auto ret = co_await producer->get_event(); ret) {
                co_await dispatch(std::move(ret.value()), dynamic_cast<NodeBase *>(producer));
            }
            else {
                break;
            }
        }
        co_return;
    }

    auto NodeManager::subscribe(int type, Consumer *consumer) -> void
    {
        _consumerMap[type].insert(consumer);
    }

    auto NodeManager::subscribe(std::vector<int> types, Consumer *consumer) -> void
    {
        for (int type : types) {
            _consumerMap[type].insert(consumer);
        }
    }

    auto NodeManager::unsubscribe(int type, Consumer *consumer) -> void
    {
        _consumerMap[type].erase(consumer);
    }

    auto NodeManager::unsubscribe(std::vector<int> types, Consumer *consumer) -> void
    {
        for (int type : types) {
            _consumerMap[type].erase(consumer);
        }
    }

    auto NodeManager::stop_node(std::string_view name) -> ilias::Task<int>
    {
        for (auto &node : _nodeList) {
            if (node.node->name() == name) {
                co_return co_await stop_node(node);
            }
        }
        co_return -1;
    }

    auto NodeManager::stop_node(NodeData &node) -> ilias::Task<int>
    {
        if (node.status == NodeStatus::eNodeStatusRunning) {
            node.status    = NodeStatus::eNodeStatusStopped;
            auto *consumer = dynamic_cast<Consumer *>(node.node.get());
            if (consumer != nullptr) {
                for (auto item : _consumerMap) {
                    // 遍历所有协议的消费者表，删除防止其运行时订阅的遗漏。
                    item.second.erase(consumer);
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
            co_return co_await node.node->disable();
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
            SPDLOG_INFO("dispatch {} to {}", proto.protoName(),
                        dynamic_cast<NodeBase *>(consumer)->name());
            co_await consumer->handle_event(proto);
        }
        co_return;
    }

    auto NodeManager::start_node() -> ilias::Task<int>
    {
        auto ret = 0;
        for (auto &node : _nodeList) {
            if (auto re = co_await start_node(node); re != 0) {
                ret = re;
                SPDLOG_ERROR("node {} start failed!", node.node->name());
            }
        }
        co_return ret;
    }

    auto NodeManager::stop_node() -> ilias::Task<int>
    {
        auto ret = 0;
        for (auto &node : _nodeList) {
            if (auto re = co_await stop_node(node); re != 0) {
                ret = re;
                SPDLOG_ERROR("node {} stop failed!", node.node->name());
            }
        }
        co_return ret;
    }

    auto NodeManager::get_nodes() -> std::vector<NodeData> &
    {
        return _nodeList;
    }

    auto NodeManager::get_nodes() const -> const std::vector<NodeData> &
    {
        return _nodeList;
    }

} // namespace mks::base