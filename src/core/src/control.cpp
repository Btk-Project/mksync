#include "mksync/core/control.hpp"
#include "mksync/proto/proto.hpp"
#include "mksync/proto/config_proto.hpp"

#include "mksync/core/app.hpp"
#include "mksync/core/mk_capture.hpp"
#include "mksync/core/mk_sender.hpp"
#include "mksync/base/default_configs.hpp"

namespace mks::base
{
    using ::ilias::Error;
    using ::ilias::Task;
    using ::ilias::TaskScope;
    using ::ilias::Unexpected;

    class VScreenCmd : public Command {
    public:
        enum Operation
        {
            eNone,
            eSetScreen,
            eRemoveScreen,
            eShowConfigs,
        };
        enum Direction
        {
            eUnknown,
            eLeft,
            eRight,
            eTop,
            eBottom,
        };

    public:
        VScreenCmd(Control *control);
        ~VScreenCmd() override = default;
        auto execute() -> Task<void> override;
        auto help() const -> std::string override;
        auto name() const -> std::string_view override;
        auto alias_names() const -> std::vector<std::string_view> override;
        void set_option(std::string_view option, std::string_view value) override;
        void set_options(const NekoProto::IProto &proto) override;
        void parser_options(const std::vector<const char *> &args) override;
        auto need_proto_type() const -> int override;

    private:
        Operation        _operation;
        Direction        _direction;
        std::string      _srcScreen;
        std::string      _dstScreen;
        Control         *_control;
        cxxopts::Options _options;
    };

    VScreenCmd::VScreenCmd(Control *control)
        : _operation(eNone), _control(control), _options("screen")
    {
        std::string ret;
        for (auto alias : alias_names()) {
            ret += alias;
            ret += ", ";
        }
        if (!ret.empty()) {
            ret.pop_back();
            ret.pop_back();
        }
        _options.custom_help(fmt::format(
            "{}{}{}{} [options...], config the screens. specification src, dst, direction to set "
            "dst near src. specification remove for src. show to print all settings.",
            this->name(), ret.empty() ? "" : "(", ret, ret.empty() ? "" : ")"));
        _options.add_options()("src", "src virtual screen name", cxxopts::value<std::string>(),
                               "[screen_name]")("dst", "dst virtual screen name",
                                                cxxopts::value<std::string>(), "[screen_name]")(
            "direction", "direction, left, right, top, bottom", cxxopts::value<std::string>(),
            "[string]")("show", "show virtual screen configs",
                        cxxopts::value<bool>()->default_value("false"))(
            "remove", "remove virtual screen", cxxopts::value<bool>()->default_value("false"));
        _options.allow_unrecognised_options();
    }

    auto VScreenCmd::execute() -> Task<void>
    {
        switch (_operation) {
        case eSetScreen:
            if (_srcScreen.empty() || _dstScreen.empty() || _direction == eUnknown) {
                SPDLOG_ERROR(
                    "please specify the src, dst and direction to set the screen position");
                break;
            }
            _control->set_virtual_screen_positions(_srcScreen, _dstScreen, _direction);
            break;
        case eRemoveScreen:
            if (_srcScreen.empty()) {
                SPDLOG_ERROR("please specify the src to remove the screen");
                break;
            }
            _control->remove_virtual_screen(_srcScreen);
            break;
        case eShowConfigs:
            _control->show_virtual_screen_positions();
            break;
        default:
            SPDLOG_ERROR("unknown operation");
            break;
        }
        _dstScreen = "";
        _srcScreen = "";
        _direction = eUnknown;
        _operation = eNone;
        co_return;
    }

    auto VScreenCmd::help() const -> std::string
    {
        return _options.help({}, false);
    }

    auto VScreenCmd::name() const -> std::string_view
    {
        return _options.program();
    }

    auto VScreenCmd::alias_names() const -> std::vector<std::string_view>
    {
        return {};
    }

    void VScreenCmd::set_option(std::string_view option, std::string_view value)
    {
        if (option == "src") {
            _srcScreen = value;
        }
        else if (option == "dst") {
            _dstScreen = value;
        }
        else if (option == "direction") {
            if (value == "left") {
                _direction = eLeft;
            }
            else if (value == "right") {
                _direction = eRight;
            }
            else if (value == "top") {
                _direction = eTop;
            }
            else if (value == "bottom") {
                _direction = eBottom;
            }
            else {
                _direction = eUnknown;
            }
        }
        else if (option == "show") {
            _operation = eShowConfigs;
        }
        else if (option == "remove") {
            _operation = eRemoveScreen;
        }
    }

    void VScreenCmd::set_options([[maybe_unused]] const NekoProto::IProto &proto)
    {
        // TODO:
    }

    void VScreenCmd::parser_options(const std::vector<const char *> &args)
    {
        auto retsult = _options.parse(args.size(), args.data());
        if (retsult.count("src") != 0U) {
            _srcScreen = retsult["src"].as<std::string>();
        }
        if (retsult.count("dst") != 0U) {
            _dstScreen = retsult["dst"].as<std::string>();
        }
        if (retsult.count("direction") != 0U) {
            auto direction = retsult["direction"].as<std::string>();
            if (direction == "left") {
                _direction = eLeft;
            }
            else if (direction == "right") {
                _direction = eRight;
            }
            else if (direction == "top") {
                _direction = eTop;
            }
            else if (direction == "bottom") {
                _direction = eBottom;
            }
            else {
                _direction = eUnknown;
            }
        }
        if (retsult.count("show") != 0U) {
            _operation = retsult["show"].as<bool>() ? eShowConfigs : _operation;
        }
        if (retsult.count("remove") != 0U) {
            _operation = retsult["remove"].as<bool>() ? eRemoveScreen : _operation;
        }
    }

    auto VScreenCmd::need_proto_type() const -> int
    {
        return 0;
    }

    Control::Control(IApp *app) : _app(app), _events(10)
    {
        auto selfScreen = _app->get_screen_info();
        _virtualScreens.insert(std::make_pair("self", selfScreen));
        _currentScreen = selfScreen.name;
    }
    Control::~Control()
    {
        _app->command_uninstaller(this);
    }

    auto Control::setup() -> ::ilias::Task<int>
    {
        _register_event_handler<ClientConnected>();
        _register_event_handler<ClientDisconnected>();
        _register_event_handler<MouseMotionEvent>();
        _register_event_handler<BorderEvent>();
        _register_event_handler<AppStatusChanged>();
        auto commandInstaller = _app->command_installer(this);
        commandInstaller(std::make_unique<VScreenCmd>(this));
        auto &settings = _app->settings();
        _vscreenConfig = settings.get(screen_settings_config_name, screen_settings_default_value);
        SPDLOG_INFO("node {}<{}> setup", name(), (void *)this);
        co_return 0;
    }

    ///> 停用节点。
    auto Control::teardown() -> ::ilias::Task<int>
    {
        _app->command_uninstaller(this);
        auto &settings = _app->settings();
        settings.set(screen_settings_config_name, _vscreenConfig);
        _syncEvent.set();
        SPDLOG_INFO("node {}<{}> teardown", name(), (void *)this);
        co_return 0;
    }

    ///> 获取节点名称。
    auto Control::name() -> const char *
    {
        return "Control";
    }

    ///> 预先订阅的事件类型集合。
    auto Control::get_subscribes() -> std::vector<int>
    {
        return _subscribes;
    }

    ///> 处理一个事件，需要订阅。
    auto Control::handle_event(const NekoProto::IProto &event) -> ::ilias::Task<void>
    {
        if (auto item = _eventHandlers.find(event.type()); item != _eventHandlers.end()) {
            co_return co_await item->second(this, event);
        }
        SPDLOG_ERROR("unhandle event {}!", event.protoName());
        co_return;
    }

    // 新客户端连接
    auto Control::handle_event(const ClientConnected &event) -> ::ilias::Task<void>
    {
        auto item = _virtualScreens.find(event.peer);
        if (item == _virtualScreens.end()) {
            item = _virtualScreens.emplace(std::make_pair(event.peer, event.info)).first;
            _screenNameTable.emplace(std::make_pair(item->second.name, item));
        }
        else {
            _virtualScreens[event.peer] = event.info;
        }
        SPDLOG_INFO("client {} connect...\nscreent name : {}\nscreen id : {}\nscreen size : "
                    "{}x{}\ntimestamp : {}",
                    event.peer, event.info.name, event.info.screenId, event.info.width,
                    event.info.height, event.info.timestamp);
        co_return;
    }

    // 客户端断开连接
    auto Control::handle_event(const ClientDisconnected &event) -> ::ilias::Task<void>
    {
        auto item = _virtualScreens.find(event.peer);
        if (item != _virtualScreens.end()) {
            _screenNameTable.erase(item->second.name);
            _virtualScreens.erase(event.peer);
        }
        SPDLOG_INFO("client {} disconnect... [{}]", event.peer, event.reason);
        co_return;
    }

    // App模式状态改变
    auto Control::handle_event(const AppStatusChanged &event) -> ::ilias::Task<void>
    {
        SPDLOG_INFO("app status changed : {}", (int)event.status);
        if (event.status == AppStatusChanged::eStarted && event.mode == AppStatusChanged::eServer) {
            _app->communication()->subscribes({
                NekoProto::ProtoFactory::protoType<MouseButtonEvent>(),
                NekoProto::ProtoFactory::protoType<MouseMotionEventConversion>(),
                NekoProto::ProtoFactory::protoType<MouseWheelEvent>(),
                NekoProto::ProtoFactory::protoType<KeyboardEvent>(),
            });
            // 这两个节点在服务端与客户分别开启即可。因此通过Control导入并加以控制。
            _captureNode = _app->node_manager().add_node(MKCapture::make(_app));
            if (auto ret = co_await _app->node_manager().setup_node(_captureNode); ret != 0) {
                SPDLOG_ERROR("setup {} node failed.", _captureNode);
            }
        }
        else if (event.status == AppStatusChanged::eStopped &&
                 event.mode == AppStatusChanged::eServer) {
            _app->communication()->unsubscribes({
                NekoProto::ProtoFactory::protoType<MouseButtonEvent>(),
                NekoProto::ProtoFactory::protoType<MouseMotionEventConversion>(),
                NekoProto::ProtoFactory::protoType<MouseWheelEvent>(),
                NekoProto::ProtoFactory::protoType<KeyboardEvent>(),
            });
            if (auto ret = co_await _app->node_manager().teardown_node(_captureNode); ret != 0) {
                SPDLOG_ERROR("teardown {} node failed.", _captureNode);
            }
            _app->node_manager().destroy_node(_captureNode);
        }
        else if (event.status == AppStatusChanged::eStarted &&
                 event.mode == AppStatusChanged::eClient) {
            _senderNode = _app->node_manager().add_node(MKSender::make(_app));
            if (auto ret = co_await _app->node_manager().setup_node(_senderNode); ret != 0) {
                SPDLOG_ERROR("setup {} node failed.", _senderNode);
            }
        }
        else if (event.status == AppStatusChanged::eStopped &&
                 event.mode == AppStatusChanged::eClient) {
            if (auto ret = co_await _app->node_manager().teardown_node(_senderNode); ret != 0) {
                SPDLOG_ERROR("teardown {} node failed.", _senderNode);
            }
            _app->node_manager().destroy_node(_senderNode);
        }
        co_return;
    }

    // 鼠标靠近屏幕边界
    auto Control::handle_event(const BorderEvent &event) -> ::ilias::Task<void>
    {
        // TODO:
        co_return;
    }

    // 鼠标移动
    auto Control::handle_event(const MouseMotionEvent &event) -> ::ilias::Task<void>
    {
        // 将鼠标位置转换到当前屏幕的位置上。并输出MouseMotionEventConversion事件。
        // TODO: 对鼠标进行处理。
        co_return;
    }

    ///> 配置屏幕信息
    auto Control::set_virtual_screen_positions(std::string_view srcScreen,
                                               std::string_view dstScreen, int direction) -> void
    {
        // TODO: 用更高效的方式存储该配置，目前的方式遍历次数有点多了。
        VirtualScreenInfo srcVs;
        if (auto item = _screenNameTable.find(srcScreen); item != _screenNameTable.end()) {
            srcVs = item->second->second;
        }
        else {
            SPDLOG_ERROR("screen {} not found.", srcScreen);
            return;
        }

        VirtualScreenInfo dstVs;
        if (auto item = _screenNameTable.find(dstScreen); item != _screenNameTable.end()) {
            dstVs = item->second->second;
        }
        else {
            SPDLOG_ERROR("screen {} not found.", dstScreen);
            return;
        }

        if (auto item =
                std::find_if(_vscreenConfig.begin(), _vscreenConfig.end(),
                             [srcScreen](const auto &sct) { return srcScreen == sct.name; });
            item != _vscreenConfig.end()) {
            item->width  = (int)srcVs.width;
            item->height = (int)srcVs.height;
            switch (direction) {
            case VScreenCmd::eLeft:
                item->left = dstScreen;
                break;
            case VScreenCmd::eRight:
                item->right = dstScreen;
                break;
            case VScreenCmd::eTop:
                item->top = dstScreen;
                break;
            case VScreenCmd::eBottom:
                item->bottom = dstScreen;
                break;
            }
        }
        else {
            _vscreenConfig.emplace_back(VirtualScreenConfig{
                .name   = srcVs.name,
                .width  = (int)srcVs.width,
                .height = (int)srcVs.height,
                .left   = direction == VScreenCmd::eLeft ? std::string(dstScreen) : "",
                .top    = direction == VScreenCmd::eTop ? std::string(dstScreen) : "",
                .right  = direction == VScreenCmd::eRight ? std::string(dstScreen) : "",
                .bottom = direction == VScreenCmd::eBottom ? std::string(dstScreen) : "",
            });
        }
        if (auto item =
                std::find_if(_vscreenConfig.begin(), _vscreenConfig.end(),
                             [dstScreen](const auto &sct) { return dstScreen == sct.name; });
            item == _vscreenConfig.end()) {
            _vscreenConfig.emplace_back(VirtualScreenConfig{
                .name   = dstVs.name,
                .width  = (int)dstVs.width,
                .height = (int)dstVs.height,
                .left   = "",
                .top    = "",
                .right  = "",
                .bottom = "",
            });
        }
        else {
            item->width  = (int)dstVs.width;
            item->height = (int)dstVs.height;
        }
    }

    ///> 展示当前配置
    auto Control::show_virtual_screen_positions() -> void
    {
        for (const auto &item : _vscreenConfig) {
            fprintf(stdout, "%s\n",
                    fmt::format("screen {} : {}x{}\n    left: {}\n     top: {}\n     right: {}\n   "
                                "  bottom: {}",
                                item.name, item.width, item.height, item.left, item.top, item.right,
                                item.bottom)
                        .c_str());
            fflush(stdout);
        }
    }

    ///> 从配置中删除屏幕
    auto Control::remove_virtual_screen(std::string_view screen) -> void
    {
        _vscreenConfig.erase(
            std::remove_if(_vscreenConfig.begin(), _vscreenConfig.end(),
                           [screen](const auto &sct) { return screen == sct.name; }));
    }

    auto Control::get_event() -> ::ilias::IoTask<NekoProto::IProto>
    {
        if (_events.size() == 0) {
            _syncEvent.clear();
            auto ret = co_await _syncEvent;
            if (!ret) {
                co_return Unexpected<Error>(ret.error());
            }
        }
        NekoProto::IProto proto;
        _events.pop(proto);
        co_return std::move(proto);
    }

    auto Control::make(App &app) -> std::unique_ptr<Control, void (*)(NodeBase *)>
    {
        return std::unique_ptr<Control, void (*)(NodeBase *)>(new Control(&app),
                                                              [](NodeBase *ptr) { delete ptr; });
    }
} // namespace mks::base