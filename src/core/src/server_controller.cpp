#include "mksync/core/controller.hpp"
#include "mksync/proto/config_proto.hpp"

#include "mksync/core/server_controller.hpp"
#include "mksync/core/mk_capture.hpp"
#include "mksync/base/default_configs.hpp"
#include "mksync/core/mk_sender.hpp"
#include "mksync/core/math_types.hpp"
#include "mksync/core/remote_controller.hpp"

MKS_BEGIN
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

public:
    VScreenCmd(ServerController *controller);
    ~VScreenCmd() override = default;
    auto execute() -> Task<std::string> override;
    auto help() const -> std::string override;
    auto name() const -> std::string_view override;
    auto alias_names() const -> std::vector<std::string_view> override;
    void set_option(std::string_view option, std::string_view value) override;
    void set_options(const NekoProto::IProto &proto) override;
    void parser_options(const std::vector<const char *> &args) override;
    auto need_proto_type() const -> int override;

private:
    Operation         _operation;
    std::string       _srcScreen;
    Point<int>        _pos;
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
    _options.custom_help(fmt::format("{}{}{}{} [options...], config the screens. specification "
                                     "--src $(screen_name) --pos $(width).$(height) to move"
                                     "specification --remove --src $(screen_name). "
                                     "specification --show to print all settings.",
                                     this->name(), ret.empty() ? "" : "(", ret,
                                     ret.empty() ? "" : ")"));
    _options.add_options()("src", "src virtual screen name", cxxopts::value<std::string>(),
                           "[screen_name]")("pos", "lefttop pos, int width.height",
                                            cxxopts::value<std::string>(), "[string]")(
        "show", "show virtual screen configs", cxxopts::value<bool>()->default_value("false"))(
        "remove", "remove virtual screen", cxxopts::value<bool>()->default_value("false"));
    _options.allow_unrecognised_options();
}

auto VScreenCmd::execute() -> Task<std::string>
{
    switch (_operation) {
    case eSetScreen:
        if (_srcScreen.empty()) {
            SPDLOG_ERROR("please specify the src, dst and direction to set the screen position");
            co_return "please specify the src, dst and direction to set the screen position";
        }
        _controller->set_virtual_screen_positions(_srcScreen, _pos);
        break;
    case eRemoveScreen:
        if (_srcScreen.empty()) {
            SPDLOG_ERROR("please specify the src to remove the screen");
            co_return "please specify the src to remove the screen";
            break;
        }
        _controller->remove_virtual_screen(_srcScreen);
        break;
    case eShowConfigs:
        co_return _controller->show_virtual_screen_positions();
    default:
        SPDLOG_ERROR("unknown operation");
        co_return "unknown operation";
    }
    _pos       = {};
    _srcScreen = "";
    _operation = eNone;
    co_return "";
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
    else if (option == "pos") {
        auto pos   = value;
        _operation = eSetScreen;
        auto split = pos.find('.');
        if (split != std::string::npos) {
            int width = 0;
            std::from_chars(pos.data(), pos.data() + split, width);
            int height = 0;
            std::from_chars(pos.data() + split + 1, pos.data() + pos.size(), height);
            _pos = {width, height};
        }
        else {
            SPDLOG_ERROR("invalid pos, should be width.height");
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
    if (retsult.count("pos") != 0U) {
        auto pos   = retsult["pos"].as<std::string>();
        _operation = eSetScreen;
        auto split = pos.find('.');
        if (split != std::string::npos) {
            int width = 0;
            std::from_chars(pos.data(), pos.data() + split, width);
            int height = 0;
            std::from_chars(pos.data() + split + 1, pos.data() + pos.size(), height);
            _pos = {width, height};
        }
        else {
            SPDLOG_ERROR("invalid pos, should be width.height");
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

ServerController::ServerController(Controller *self, IApp *app)
    : ControllerImp(self, app), _sender(MKSender::make(app))
{
}

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
    if (co_await _sender->setup() != 0) {
        co_return 0;
    }
    co_await _sender->start_sender();
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
    auto commandInstaller = _app->command_installer(_self);
    commandInstaller(std::make_unique<VScreenCmd>(this));
    auto &settings  = _app->settings();
    _vscreenConfig  = settings.get(screen_settings_config_name, screen_settings_default_value);
    auto selfScreen = _app->get_screen_info();
    if (auto item = _virtualScreens.insert(std::make_pair("self", selfScreen)); item.second) {
        _screenNameTable.insert(std::make_pair(selfScreen.name, item.first));
    }
    _currentScreen.name       = selfScreen.name;
    _currentScreen.peer       = "self";
    _currentScreen.isInBorder = false;
    _currentScreen.config     = nullptr;
    _currentScreen.posX       = 0;
    _currentScreen.posY       = 0;
    if (auto item =
            std::find_if(_vscreenConfig.begin(), _vscreenConfig.end(),
                         [&self = selfScreen.name](auto &item) { return item.name == self; });
        item != _vscreenConfig.end()) {
        _currentScreen.config = std::addressof(*item);
    }
    else {
        _currentScreen.config = std::addressof(*_vscreenConfig.emplace(
            _vscreenConfig.end(), VirtualScreenConfig{.name   = selfScreen.name,
                                                      .posX   = 0,
                                                      .posY   = 0,
                                                      .width  = (int)selfScreen.width,
                                                      .height = (int)selfScreen.height}));
    }
    _app->node_manager().subscribe(_subscribes, _self);
    _app->communication()->subscribes({
        NekoProto::ProtoFactory::protoType<MouseButtonEvent>(),
        NekoProto::ProtoFactory::protoType<MouseMotionEventConversion>(),
        NekoProto::ProtoFactory::protoType<MouseWheelEvent>(),
        NekoProto::ProtoFactory::protoType<KeyboardEvent>(),
    });

    auto *rpcModule =
        dynamic_cast<RemoteController *>(_app->node_manager().get_node("RemoteController"));
    if (rpcModule != nullptr) {
        auto &rpcServer                   = rpcModule->rpc_server();
        rpcServer->setVirtualScreenConfig = [this](VirtualScreenConfig vsconfig) {
            set_virtual_screen_positions(vsconfig.name, {vsconfig.posX, vsconfig.posY});
        };
        rpcServer->setVirtualScreenConfigs = [this](std::vector<VirtualScreenConfig> vsconfigs) {
            for (auto &vsconfig : vsconfigs) {
                set_virtual_screen_positions(vsconfig.name, {vsconfig.posX, vsconfig.posY});
            }
        };

        rpcServer->removeVirtualScreen = [this](std::string_view screen) {
            remove_virtual_screen(screen);
        };

        rpcServer->getOnlineScreens = [this]() -> std::vector<VirtualScreenInfo> {
            std::vector<VirtualScreenInfo> configs;
            for (auto &[peer, vs] : _virtualScreens) {
                if (peer != "self") {
                    configs.push_back(vs);
                }
            }
            return configs;
        };
    }

    co_return 0;
}

auto ServerController::teardown() -> ::ilias::Task<int>
{
    if (_captureNode.empty()) {
        co_return 0;
    }
    auto *rpcModule =
        dynamic_cast<RemoteController *>(_app->node_manager().get_node("RemoteController"));
    if (rpcModule != nullptr) {
        auto &rpcServer = rpcModule->rpc_server();
        rpcServer->setVirtualScreenConfig.clear();
        rpcServer->setVirtualScreenConfigs.clear();
        rpcServer->getOnlineScreens.clear();
    }
    co_await _sender->teardown();
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

auto ServerController::reconfigure([[maybe_unused]] Settings &settings) -> ::ilias::Task<void>
{
    // TODO: 哪些配置可以动态修改？
    co_return;
}

///> 配置屏幕信息
auto ServerController::set_virtual_screen_positions(std::string_view srcScreen, Point<int> pos)
    -> void
{
    VirtualScreenInfo srcVs;
    if (auto item = _screenNameTable.find(srcScreen); item != _screenNameTable.end()) {
        srcVs = item->second->second;
    }
    else {
        SPDLOG_ERROR("screen {} not found.", srcScreen);
        return;
    }
    if (auto item = std::find_if(_vscreenConfig.begin(), _vscreenConfig.end(),
                                 [srcScreen](const auto &sct) { return srcScreen == sct.name; });
        item != _vscreenConfig.end()) {
        item->posX   = pos.x;
        item->posY   = pos.y;
        item->width  = (int)srcVs.width;
        item->height = (int)srcVs.height;
    }
    else {
        _vscreenConfig.emplace_back(VirtualScreenConfig{
            .name   = srcVs.name,
            .posX   = pos.x,
            .posY   = pos.y,
            .width  = (int)srcVs.width,
            .height = (int)srcVs.height,
        });
    }
}
///> 展示当前配置
auto ServerController::show_virtual_screen_positions() -> std::string
{
    std::string result;
    result = fmt::format("---------- virtual screens -----------\n");
    for (const auto &screen : _virtualScreens) {
        result += fmt::format("screen {}({}) : {}x{}\n", screen.second.name, screen.second.screenId,
                              screen.second.width, screen.second.height);
    }
    result += "---------- screens config ----------\n";
    for (const auto &item : _vscreenConfig) {
        result += fmt::format("screen {} : {}x{} - {}x{}\n", item.name, item.posX, item.posY,
                              item.width, item.height);
    }
    result += "---------------------------------------\n";
    fprintf(stdout, "%s", result.c_str());
    fflush(stdout);
    return result;
}
///> 从配置中删除屏幕
auto ServerController::remove_virtual_screen(std::string_view screen) -> void
{
    SPDLOG_INFO("remove virtual screen {}", screen);
    _vscreenConfig.erase(std::remove_if(_vscreenConfig.begin(), _vscreenConfig.end(),
                                        [screen](const auto &sct) { return screen == sct.name; }));
}

auto ServerController::set_current_screen(std::string_view screen) -> Task<bool>
{
    if (screen == _currentScreen.name) {
        co_return false;
    }
    // 切换屏幕
    if (auto item = _screenNameTable.find(screen); item != _screenNameTable.end()) {
        co_await _app->push_event(FocusScreenChanged::emplaceProto(item->first, item->second->first,
                                                                   _currentScreen.name,
                                                                   _currentScreen.peer),
                                  _self);
        if (item->second->first != "self") {
            co_await _app->push_event(CaptureControl::emplaceProto(CaptureControl::eStart), _self);
        }
        else {
            co_await _app->push_event(CaptureControl::emplaceProto(CaptureControl::eStop), _self);
        }
        _currentScreen.name = item->second->second.name;
        _currentScreen.peer = item->second->first;
        SPDLOG_INFO("switch to screen {}", screen);
    }
    else {
        if (!screen.empty()) {
            SPDLOG_ERROR("screen {} not online.", screen);
        }
        co_return false;
    }
    if (auto item = std::find_if(_vscreenConfig.begin(), _vscreenConfig.end(),
                                 [screen](auto &item) { return item.name == screen; });
        item != _vscreenConfig.end()) {
        _currentScreen.config = std::addressof(*item);
        co_return true;
    }
    _currentScreen.isInBorder = true; // 标记防止马上就被识别成在屏幕边界。
    _currentScreen.posX       = 0;
    _currentScreen.posY       = 0;
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
    if (event.peer == _currentScreen.peer) {
        co_await set_current_screen(_virtualScreens["self"].name);
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
    Point pt(event.x + _currentScreen.config->posX, event.y + _currentScreen.config->posY);
    if (event.border == BorderEvent::eLeft) {
        pt.x = _currentScreen.config->posX - 1;
    }
    else if (event.border == BorderEvent::eRight) {
        pt.x = _currentScreen.config->posX + _currentScreen.config->width + 1;
    }
    else if (event.border == BorderEvent::eTop) {
        pt.y = _currentScreen.config->posY - 1;
    }
    else if (event.border == BorderEvent::eBottom) {
        pt.y = _currentScreen.config->posY + _currentScreen.config->height + 1;
    }
    else {
        co_return;
    }
    SPDLOG_INFO("Mouse Border: {}, x {}, y {}", (int)event.border, pt.x, pt.y);
    std::string_view nextScreen;
    auto             prevConfig = *_currentScreen.config;
    for (const auto &screen : _vscreenConfig) {
        if (screen.name == _currentScreen.name) {
            continue;
        }
        Rect rect = Rect(screen.posX, screen.posY, screen.width, screen.height);
        if (rect.contains(pt)) {
            nextScreen = screen.name;
            break;
        }
    }
    if (co_await set_current_screen(nextScreen)) {
        ILIAS_ASSERT(_currentScreen.config != nullptr);
        switch (event.border) { // 计算从上一个屏幕边界出去的鼠标应该从下一块屏幕的何处进入。
        case BorderEvent::eLeft:
            _currentScreen.posX = _currentScreen.config->width;
            _currentScreen.posY = (int)(event.y + prevConfig.posY - _currentScreen.config->posY);
            break;
        case BorderEvent::eRight:
            _currentScreen.posX = 0;
            _currentScreen.posY = (int)(event.y + prevConfig.posY - _currentScreen.config->posY);
            break;
        case BorderEvent::eTop:
            _currentScreen.posX = (int)(event.x + prevConfig.posX - _currentScreen.config->posX);
            _currentScreen.posY = _currentScreen.config->height;
            break;
        case BorderEvent::eBottom:
            _currentScreen.posX = (int)(event.x + prevConfig.posX - _currentScreen.config->posX);
            _currentScreen.posY = 0;
            break;
        }
        if (_currentScreen.peer == "self") {
            SPDLOG_INFO("Mouse to: x {}, y {}", _currentScreen.posX, _currentScreen.posY);
            co_await _sender->handle_event(MouseMotionEventConversion::emplaceProto(
                _currentScreen.posX, _currentScreen.posY, true, 0U));
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
    _currentScreen.posX = std::clamp((int)((event.isAbsolute ? 0 : _currentScreen.posX) + event.x),
                                     0, _currentScreen.config->width);
    _currentScreen.posY = std::clamp((int)((event.isAbsolute ? 0 : _currentScreen.posY) + event.y),
                                     0, _currentScreen.config->height);
    if (_currentScreen.isInBorder) {
        if (_currentScreen.posX > 10 && _currentScreen.posX < _currentScreen.config->width - 10 &&
            _currentScreen.posY > 10 && _currentScreen.posY < _currentScreen.config->height - 10) {
            _currentScreen.isInBorder = false;
        }
    }
    else { // 移动到了屏幕边界，立刻跳出当前屏幕到下一个屏幕。
        BorderEvent::Border border =
            BorderEvent::check_border(_currentScreen.posX, _currentScreen.posY,
                                      _currentScreen.config->width, _currentScreen.config->height);
        if (border != 0) {
            _currentScreen.isInBorder = true;
            auto oldScreen            = _currentScreen.name;
            co_await handle_event(mks::BorderEvent::emplaceProto(
                0U, (uint32_t)border, _currentScreen.posX, _currentScreen.posY));
            if (oldScreen != _currentScreen.name) {
                co_return; // 跳转到了新的屏幕，不再继续处理当前的鼠标移动事件。
            }
        }
    }
    // 构建用于发送到客户端的鼠标移动事件
    // SPDLOG_INFO("MouseMotionEvent: x {}, y {}", _currentScreen.posX, _currentScreen.posY);
    co_await _app->push_event(MouseMotionEventConversion::emplaceProto(
                                  _currentScreen.posX, _currentScreen.posY, true, event.timestamp),
                              _self);
    co_return;
}
MKS_END