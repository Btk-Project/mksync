#pragma once

#include "preinclude.hpp"
#include "refl/formatter.hpp"
#include "core.hpp"
#include <ilias/net.hpp>

MKS_BEGIN

using ilias::IPEndpoint;

/**
 * @brief One screen registered into the server topology and routing tables.
 *
 * Topology adjacency uses @c key + @c cell. Input routing uses @c endpoint to
 * find the client sender, and @c info for real pixel rects / entry mapping.
 *
 * Instances live inside @ref ServerScreenStore (multimap nodes). Pointers such
 * as the input router's active screen must be cleared before those nodes are
 * erased.
 */
struct VirtualScreen {
    /** TCP peer that owns this screen (server local endpoint for local screens). */
    IPEndpoint endpoint;
    /** Stable identity: machineId (preferred) or endpoint string + index. */
    ScreenKey key;
    /** Integer grid cell for neighbor queries (not pixels). */
    GridPosition cell;
    /** True when the screen belongs to the host Server process. */
    bool local = false;
    /** Client-reported real geometry used for hit-testing and injection. */
    ScreenInfo info;
};

inline auto _refl_fmt_inline(const VirtualScreen &value, auto it) {
    return fmtlib::format_to(
        it,
        "VirtualScreen {{ endpoint: {}, key: {}, cell: {}, local: {}, info: {} }}",
        value.endpoint,
        value.key,
        value.cell,
        value.local,
        value.info
    );
}

MKS_END
