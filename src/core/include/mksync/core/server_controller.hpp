/**
 * @file controller.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-03-15
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include "mksync/core/core_library.h"
#include "mksync/core/controller.hpp"
#include "mksync/core/math_types.hpp"

MKS_BEGIN
class MKSender;
class MKS_CORE_API ServerController final : public ControllerImp {
public:
    ServerController(Controller *self, IApp *app);
    virtual ~ServerController();
    [[nodiscard("coroutine function")]]
    auto setup() -> ::ilias::Task<int> override;
    ///> 停用节点。
    [[nodiscard("coroutine function")]]
    auto teardown() -> ::ilias::Task<int> override;
    [[nodiscard("coroutine function")]]
    auto handle_event(const NekoProto::IProto &event) -> ::ilias::Task<void> override;
    auto reconfigure([[maybe_unused]] Settings &settings) -> ::ilias::Task<void> override;
    auto type() -> Type override { return eServerController; }

public:
    ///> 配置屏幕信息
    auto set_virtual_screen_positions(std::string_view srcScreen, Point<int> pos) -> void;
    ///> 展示当前配置
    auto show_virtual_screen_positions() -> std::string;
    ///> 从配置中删除屏幕
    auto remove_virtual_screen(std::string_view screen) -> void;
    auto set_current_screen(std::string_view screen) -> ::ilias::Task<bool>;

public:
    [[nodiscard("coroutine function")]]
    auto handle_event(const ClientConnected &event) -> ::ilias::Task<void>;
    [[nodiscard("coroutine function")]]
    auto handle_event(const ClientDisconnected &event) -> ::ilias::Task<void>;
    [[nodiscard("coroutine function")]]
    auto handle_event(const BorderEvent &event) -> ::ilias::Task<void>;
    [[nodiscard("coroutine function")]]
    auto handle_event(const MouseMotionEvent &event) -> ::ilias::Task<void>;

private:
    template <typename T>
    auto _register_event_handler() -> void;

private:
    std::string_view                                _captureNode;
    std::vector<VirtualScreenConfig>                _vscreenConfig;
    std::vector<int>                                _subscribes;
    std::unique_ptr<MKSender, void (*)(NodeBase *)> _sender;
    std::unordered_map<int, ::ilias::Task<void> (*)(ServerController *, const NekoProto::IProto &)>
                                                          _eventHandlers;
    std::map<std::string, VirtualScreenInfo, std::less<>> _virtualScreens; // peer -> vscreninfo
    struct {
        std::string          peer;
        std::string          name;
        VirtualScreenConfig *config;
        int32_t              posX; // 所在屏幕的X位置
        int32_t              posY; // 所在屏幕的Y位置
        bool                 isInBorder;
    } _currentScreen = {"", "", nullptr, 0, 0, false};
    std::map<std::string, std::map<std::string, VirtualScreenInfo, std::less<>>::iterator,
             std::less<>>
        _screenNameTable; // screen_name -> vscreeninfo
};

template <typename T>
auto ServerController::_register_event_handler() -> void
{
    _subscribes.push_back(NekoProto::ProtoFactory::protoType<T>());
    _eventHandlers.insert(std::make_pair(
        NekoProto::ProtoFactory::protoType<T>(),
        [](ServerController *controller, const NekoProto::IProto &event) -> ::ilias::Task<void> {
            const auto *ev = event.cast<T>();
            if (ev != nullptr) {
                co_await controller->handle_event(*ev);
            }
            co_return;
        }));
}
MKS_END