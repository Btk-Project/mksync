#include "core.hpp"
#include "refl/formatter.hpp"
#include "refl/enum.hpp"

MKS_BEGIN

ENUM_FORMATTER_IMPL(MouseButton);
ENUM_FORMATTER_IMPL(Key);
FLAGS_FORMATTER_IMPL(KeyModifier);

MKS_END