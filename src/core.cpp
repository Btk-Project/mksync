#include "core.hpp"
#include "refl/formatter.hpp"
#include "refl/enum.hpp"

MKS_BEGIN

// Mouse
FORMATTER_IMPL(MouseButton);

// Key
FORMATTER_IMPL(Key);
FLAGS_FORMATTER_IMPL(KeyModifier);

// Event
FORMATTER_IMPL(MouseMoveEvent);
FORMATTER_IMPL(MouseButtonEvent);
FORMATTER_IMPL(MouseWheelEvent);
FORMATTER_IMPL(KeyEvent);
FORMATTER_IMPL(ScreenInfo);
FORMATTER_IMPL(ScreenChangeEvent);

MKS_END