#include <mksync/_proto/message.hpp>
// Standard Library
#include <type_traits>
// System Library
// Third-Party Library
// Local Library

MKS_BEGIN
MKS_PROTO_BEGIN

auto messageType(const Message &message) -> MessageType
{
    return std::visit(
        [](const auto &value) -> MessageType {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, Ack>) {
                return MessageType::eAck;
            }
            else if constexpr (std::is_same_v<T, Hello>) {
                return MessageType::eHello;
            }
            else if constexpr (std::is_same_v<T, Bye>) {
                return MessageType::eBye;
            }
            else if constexpr (std::is_same_v<T, Ping>) {
                return MessageType::ePing;
            }
            else if constexpr (std::is_same_v<T, Pong>) {
                return MessageType::ePong;
            }
            else if constexpr (std::is_same_v<T, ScreenInfo>) {
                return MessageType::eScreenInfo;
            }
            else if constexpr (std::is_same_v<T, ScreenEnter>) {
                return MessageType::eScreenEnter;
            }
            else if constexpr (std::is_same_v<T, ScreenLeave>) {
                return MessageType::eScreenLeave;
            }
            else if constexpr (std::is_same_v<T, MouseDown>) {
                return MessageType::eMouseDown;
            }
            else if constexpr (std::is_same_v<T, MouseUp>) {
                return MessageType::eMouseUp;
            }
            else if constexpr (std::is_same_v<T, MouseRepeat>) {
                return MessageType::eMouseRepeat;
            }
            else if constexpr (std::is_same_v<T, MouseMove>) {
                return MessageType::eMouseMove;
            }
            else if constexpr (std::is_same_v<T, MouseWheel>) {
                return MessageType::eMouseWheel;
            }
            else if constexpr (std::is_same_v<T, KeyboardDown>) {
                return MessageType::eKeyboardDown;
            }
            else if constexpr (std::is_same_v<T, KeyboardUp>) {
                return MessageType::eKeyboardUp;
            }
            else if constexpr (std::is_same_v<T, KeyboardRepeat>) {
                return MessageType::eKeyboardRepeat;
            }
            else {
                return MessageType::eError;
            }
        },
        message);
}

auto toMessage(const platform::InputEvent &event) -> std::optional<Message>
{
    using Type = platform::InputEvent::Type;

    switch (event.type) {
    case Type::MouseMove:
        if (auto data = event.getIf<platform::InputEvent::MouseMoveData>()) {
            return Message{
                MouseMove{.screenIndex = event.metadata.screenIndex, .x = data->x, .y = data->y}
            };
        }
        break;
    case Type::MousePress:
    case Type::MouseRelease:
        if (auto data = event.getIf<platform::InputEvent::MouseButtonData>()) {
            return Message{
                MouseButton{.screenIndex = event.metadata.screenIndex,
                            .x           = data->x,
                            .y           = data->y,
                            .button      = data->button}
            };
        }
        break;
    case Type::MouseWheel:
        if (auto data = event.getIf<platform::InputEvent::MouseWheelData>()) {
            return Message{
                MouseWheel{.screenIndex = event.metadata.screenIndex,
                           .x           = data->x,
                           .y           = data->y,
                           .deltaX      = data->deltaX,
                           .deltaY      = data->deltaY}
            };
        }
        break;
    case Type::KeyPress:
    case Type::KeyRelease:
        if (auto data = event.getIf<platform::InputEvent::KeyData>()) {
            return Message{
                KeyEvent{.key        = data->key,
                         .modifiers  = data->modifiers,
                         .nativeCode = data->nativeCode,
                         .repeat     = data->repeat}
            };
        }
        break;
    default:
        break;
    }
    return std::nullopt;
}

auto toInputEvent(MessageType type, const Message &message) -> std::optional<platform::InputEvent>
{
    using Event = platform::InputEvent;
    return std::visit(
        [&](const auto &value) -> std::optional<Event> {
            using T = std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<T, MouseMove>) {
                if (type != MessageType::MouseMove) {
                    return std::nullopt;
                }
                return Event{
                    .type     = Event::Type::MouseMove,
                    .metadata = {.screenIndex = value.screenIndex},
                    .payload  = Event::MouseMoveData{.x = value.x, .y = value.y}
                };
            }
            else if constexpr (std::is_same_v<T, MouseButton>) {
                if (type != MessageType::MousePress && type != MessageType::MouseRelease) {
                    return std::nullopt;
                }
                return Event{
                    .type     = type == MessageType::MousePress ? Event::Type::MousePress
                                                                : Event::Type::MouseRelease,
                    .metadata = {.screenIndex = value.screenIndex},
                    .payload =
                        Event::MouseButtonData{.x = value.x, .y = value.y, .button = value.button},
                };
            }
            else if constexpr (std::is_same_v<T, MouseWheel>) {
                if (type != MessageType::MouseWheel) {
                    return std::nullopt;
                }
                return Event{
                    .type     = Event::Type::MouseWheel,
                    .metadata = {.screenIndex = value.screenIndex},
                    .payload  = Event::MouseWheelData{.x      = value.x,
                                 .y      = value.y,
                                 .deltaX = value.deltaX,
                                 .deltaY = value.deltaY}
                };
            }
            else if constexpr (std::is_same_v<T, KeyEvent>) {
                if (type != MessageType::KeyPress && type != MessageType::KeyRelease) {
                    return std::nullopt;
                }
                return Event{
                    .type     = type == MessageType::KeyPress ? Event::Type::KeyPress
                                                              : Event::Type::KeyRelease,
                    .metadata = {},
                    .payload  = Event::KeyData{.key        = value.key,
                                 .modifiers  = value.modifiers,
                                 .nativeCode = value.nativeCode,
                                 .repeat     = value.repeat},
                };
            }
            else {
                return std::nullopt;
            }
        },
        message);
}

MKS_PROTO_END
MKS_END
