#include "mksync/core/controller.hpp"
#include "mksync/proto/config_proto.hpp"

#include "mksync/core/server_controller.hpp"
#include "mksync/core/mk_capture.hpp"
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
        VScreenCmd(ServerController *controller);
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
        Operation         _operation;
        Direction         _direction;
        std::string       _srcScreen;
        std::string       _dstScreen;
        ServerController *_controller;
        cxxopts::Options  _options;
    };

    VScreenCmd::VScreenCmd(ServerController *controller)
        : _operation(eNone), _controller(controller), _options("screen")
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
            _controller->set_virtual_screen_positions(_srcScreen, _dstScreen, _direction);
            break;
        case eRemoveScreen:
            if (_srcScreen.empty()) {
                SPDLOG_ERROR("please specify the src to remove the screen");
                break;
            }
            _controller->remove_virtual_screen(_srcScreen);
            break;
        case eShowConfigs:
            _controller->show_virtual_screen_positions();
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
        auto retsult = _options.parse((int)args.size(), args.data());
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

    ServerController::ServerController(Controller *self, IApp *app) : ControllerImp(self, app) {}

    ServerController::~ServerController()
    {
        if (!_captureNode.empty()) { // 收个尾
            teardown().wait();
        }
    }

    auto ServerController::setup() -> ::ilias::Task<int>
    {
        if (!_captureNode.empty()) {
            co_return 0;
        }
        // 这两个节点在服务端与客户分别开启即可。因此通过Controller导入并加以控制。
        _captureNode = _app->node_manager().add_node(MKCapture::make(_app));
        if (auto ret = co_await _app->node_manager().setup_node(_captureNode); ret != 0) {
            SPDLOG_ERROR("setup {} node failed.", _captureNode);
            _captureNode = "";
            co_return ret;
        }
        _register_event_handler<ClientConnected>();
        _register_event_handler<ClientDisconnected>();
        _register_event_handler<MouseMotionEvent>();
        _register_event_handler<BorderEvent>();
        // _register_event_handler<AppStatusChanged>();
        // _subscribes.pop_back(); // AppStatusChanged在主节点里面被订阅了，弹出防止其被取消订阅。
        auto commandInstaller = _app->command_installer(_self);
        commandInstaller(std::make_unique<VScreenCmd>(this));
        auto &settings  = _app->settings();
        _vscreenConfig  = settings.get(screen_settings_config_name, screen_settings_default_value);
        auto selfScreen = _app->get_screen_info();
        _virtualScreens.insert(std::make_pair("self", selfScreen));
        _currentScreen.name       = selfScreen.name;
        _currentScreen.peer       = "self";
        _currentScreen.isInBorder = false;
        _currentScreen.config     = nullptr;
        _currentScreen.posX       = 0;
        _currentScreen.posY       = 0;
        if (auto item = std::find_if(_vscreenConfig.begin(), _vscreenConfig.end(),
                                     [](auto &item) { return item.name == "self"; });
            item != _vscreenConfig.end()) {
            _currentScreen.config = std::addressof(*item);
        }
        else {
            _currentScreen.config = std::addressof(*_vscreenConfig.emplace(
                _vscreenConfig.end(), VirtualScreenConfig{.name   = "self",
                                                          .width  = (int)selfScreen.width,
                                                          .height = (int)selfScreen.height,
                                                          .left   = "",
                                                          .top    = "",
                                                          .right  = "",
                                                          .bottom = ""}));
        }
        _app->node_manager().subscribe(_subscribes, _self);
        _app->communication()->subscribes({
            NekoProto::ProtoFactory::protoType<MouseButtonEvent>(),
            NekoProto::ProtoFactory::protoType<MouseMotionEventConversion>(),
            NekoProto::ProtoFactory::protoType<MouseWheelEvent>(),
            NekoProto::ProtoFactory::protoType<KeyboardEvent>(),
        });
        co_return 0;
    }

    auto ServerController::teardown() -> ::ilias::Task<int>
    {
        if (_captureNode.empty()) {
            co_return 0;
        }
        _app->node_manager().unsubscribe(_subscribes, _self);
        _app->command_uninstaller(_self);
        auto &settings = _app->settings();
        settings.set(screen_settings_config_name, _vscreenConfig);
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
        _captureNode = "";
        co_return 0;
    }

    auto ServerController::handle_event(const NekoProto::IProto &event) -> ::ilias::Task<void>
    {
        if (auto item = _eventHandlers.find(event.type()); item != _eventHandlers.end()) {
            co_return co_await item->second(this, event);
        }
        SPDLOG_WARN("ServerController unhandle event {}!", event.protoName());
        co_return;
    }

    ///> 配置屏幕信息
    auto ServerController::set_virtual_screen_positions(std::string_view srcScreen,
                                                        std::string_view dstScreen, int direction)
        -> void
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
                .left   = direction == VScreenCmd::eRight ? std::string(srcScreen) : "",
                .top    = direction == VScreenCmd::eBottom ? std::string(srcScreen) : "",
                .right  = direction == VScreenCmd::eLeft ? std::string(srcScreen) : "",
                .bottom = direction == VScreenCmd::eTop ? std::string(srcScreen) : "",
            });
        }
        else {
            item->width  = (int)dstVs.width;
            item->height = (int)dstVs.height;
            switch (direction) {
            case VScreenCmd::eLeft:
                item->right = srcScreen;
                break;
            case VScreenCmd::eRight:
                item->left = srcScreen;
                break;
            case VScreenCmd::eTop:
                item->bottom = srcScreen;
                break;
            case VScreenCmd::eBottom:
                item->top = srcScreen;
            }
        }
    }
    ///> 展示当前配置
    auto ServerController::show_virtual_screen_positions() -> void
    {
        for (const auto &item : _vscreenConfig) {
            fprintf(stdout, "%s\n",
                    fmt::format("screen {} : {}x{}\n    left: {}\n    top: {}\n    right: {}\n  "
                                "  bottom: {}",
                                item.name, item.width, item.height, item.left, item.top, item.right,
                                item.bottom)
                        .c_str());
            fflush(stdout);
        }
    }
    ///> 从配置中删除屏幕
    auto ServerController::remove_virtual_screen(std::string_view screen) -> void
    {
        SPDLOG_INFO("remove virtual screen {}", screen);
        _vscreenConfig.erase(
            std::remove_if(_vscreenConfig.begin(), _vscreenConfig.end(),
                           [screen](const auto &sct) { return screen == sct.name; }));
        for (auto &item : _vscreenConfig) {
            if (item.left == screen) {
                item.left = "";
            }
            if (item.top == screen) {
                item.top = "";
            }
            if (item.right == screen) {
                item.right = "";
            }
            if (item.bottom == screen) {
                item.bottom = "";
            }
        }
    }

    auto ServerController::set_current_screen(std::string_view screen) -> Task<bool>
    {
        if (screen == _currentScreen.name) {
            co_return false;
        }
        // 切换屏幕
        if (auto item = _screenNameTable.find(screen); item != _screenNameTable.end()) {
            co_await _app->push_event(
                FocusScreenChanged::emplaceProto(item->second->second.name, item->second->first,
                                                 _currentScreen.name, _currentScreen.peer),
                _self);
            _currentScreen.name = item->second->second.name;
            _currentScreen.peer = item->second->first;
            SPDLOG_INFO("switch to screen {}", screen);
        }
        else {
            SPDLOG_ERROR("screen {} not found.", screen);
            co_return false;
        }
        if (auto item = std::find_if(_vscreenConfig.begin(), _vscreenConfig.end(),
                                     [screen](auto &item) { return item.name == screen; });
            item != _vscreenConfig.end()) {
            _currentScreen.config = std::addressof(*item);
            co_return true;
        }
        SPDLOG_CRITICAL("near screen {} config not found.", screen);
        co_return false;
    }

    auto ServerController::handle_event(const ClientConnected &event) -> ::ilias::Task<void>
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

    auto ServerController::handle_event(const ClientDisconnected &event) -> ::ilias::Task<void>
    {
        auto item = _virtualScreens.find(event.peer);
        if (item != _virtualScreens.end()) {
            _screenNameTable.erase(item->second.name);
            _virtualScreens.erase(event.peer);
        }
        SPDLOG_INFO("client {} disconnect... [{}]", event.peer, event.reason);
        co_return;
    }

    auto ServerController::handle_event(const BorderEvent &event) -> ::ilias::Task<void>
    {
        if (_currentScreen.config == nullptr) {
            SPDLOG_ERROR("current screen config is null!!!");
            co_return;
        }
        std::string_view nextScreen;
        if (event.border == BorderEvent::eLeft && !_currentScreen.config->left.empty()) {
            nextScreen = _currentScreen.config->left;
        }
        else if (event.border == BorderEvent::eRight && !_currentScreen.config->right.empty()) {
            nextScreen = _currentScreen.config->right;
        }
        else if (event.border == BorderEvent::eTop && !_currentScreen.config->top.empty()) {
            nextScreen = _currentScreen.config->top;
        }
        else if (event.border == BorderEvent::eBottom && !_currentScreen.config->bottom.empty()) {
            nextScreen = _currentScreen.config->bottom;
        }
        else {
            co_return;
        }
        if (set_current_screen(nextScreen)) {
            _currentScreen.isInBorder = true; // 标记防止马上就被识别成在屏幕边界。
            ILIAS_ASSERT(_currentScreen.config != nullptr);
            switch (event.border) { // 计算从上一个屏幕边界出去的鼠标应该从下一块屏幕的何处进入。
            case BorderEvent::eLeft:
                _currentScreen.posX = _currentScreen.config->width;
                _currentScreen.posY = (int)(event.y * _currentScreen.config->height);
                break;
            case BorderEvent::eRight:
                _currentScreen.posX = 0;
                _currentScreen.posY = (int)(event.y * _currentScreen.config->height);
                break;
            case BorderEvent::eTop:
                _currentScreen.posX = (int)(event.x * _currentScreen.config->width);
                _currentScreen.posY = _currentScreen.config->height;
                break;
            case BorderEvent::eBottom:
                _currentScreen.posX = (int)(event.x * _currentScreen.config->width);
                _currentScreen.posY = 0;
                break;
            }
        }
        co_return;
    }

    auto ServerController::handle_event(const MouseMotionEvent &event) -> ::ilias::Task<void>
    {
        if (_currentScreen.config == nullptr || _currentScreen.peer == "self") {
            SPDLOG_ERROR("current screen config is null or is self!!!");
            co_return;
        }
        // 将鼠标位置转换到当前屏幕的位置上。并输出MouseMotionEventConversion事件。
        _currentScreen.posX = (int)((event.isAbsolute ? _currentScreen.posX : 0) +
                                    (event.x * _currentScreen.config->width));
        _currentScreen.posY = (int)((event.isAbsolute ? _currentScreen.posY : 0) +
                                    (event.y * _currentScreen.config->height));
        if (_currentScreen.isInBorder) {
            if (_currentScreen.posX > 10 &&
                _currentScreen.posX < _currentScreen.config->width - 10 &&
                _currentScreen.posY > 10 &&
                _currentScreen.posY < _currentScreen.config->height - 10) {
                _currentScreen.isInBorder = false;
            }
        }
        else { // 移动到了屏幕边界，立刻跳出当前屏幕到下一个屏幕。
            BorderEvent::Border border = BorderEvent::check_border(
                _currentScreen.posX, _currentScreen.posY, _currentScreen.config->width,
                _currentScreen.config->height);
            if (border != 0) {
                SPDLOG_INFO("Mouse Border: {}, x {}, y {}", (int)border, _currentScreen.posX,
                            _currentScreen.posY);
                _currentScreen.isInBorder = true;
                auto oldScreen            = _currentScreen.name;
                co_await handle_event(mks::BorderEvent::emplaceProto(
                    0U, (uint32_t)border, (float)_currentScreen.posX / _currentScreen.config->width,
                    (float)_currentScreen.posY / _currentScreen.config->height));
                if (oldScreen != _currentScreen.name) {
                    co_return; // 跳转到了新的屏幕，不再继续处理当前的鼠标移动事件。
                }
            }
        }
        // 构建用于发送到客户端的鼠标移动事件
        co_await _app->push_event(MouseMotionEventConversion::emplaceProto(
                                      (float)_currentScreen.posX / _currentScreen.config->width,
                                      (float)_currentScreen.posY / _currentScreen.config->height,
                                      true, event.timestamp),
                                  _self);
        co_return;
    }

} // namespace mks::base