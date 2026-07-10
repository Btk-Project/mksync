#pragma once

#include "preinclude.hpp"
#include "config/app_config.hpp"
#include "core.hpp"
#include "server_types.hpp"
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

MKS_BEGIN

/**
 * @brief Topology cells, VirtualScreen routes, owner ids, and layout config.
 *
 * Responsibilities:
 * - Register / replace / remove screens for an endpoint.
 * - Map @c ScreenKey to square-grid neighbors via @ref ScreenTopology.
 * - Persist layout cells into @ref AppConfig when a config path is set.
 * - Resolve owner id (local machineId vs remote Hello machineId vs endpoint).
 *
 * Non-responsibilities:
 * - Which screen is currently active for keyboard/mouse (@ref ServerInputRouter).
 * - TCP sessions (@ref ServerSession).
 *
 * @note Callers that hold a @c VirtualScreen* (active screen) must clear it
 *       before @c removeScreen erases the underlying multimap node.
 */
class ServerScreenStore {
public:
    ServerScreenStore() = default;
    /**
     * @param config     Loaded or default app config (machineId, trusted list, layout).
     * @param configPath When non-empty, successful layout changes are saved here.
     */
    ServerScreenStore(AppConfig config, std::filesystem::path configPath);

    auto config() const -> const AppConfig &;
    auto topologyScreens() const -> std::vector<TopologyScreen>;
    auto topology() const -> const ScreenTopology &;

    /**
     * @brief Add screens for @p endpoint (does not remove previous entries).
     *
     * Prefer Server::registerScreens, which removes first and coordinates
     * active-screen pointers. Layout cells come from config when present,
     * otherwise local primary at (0,0) and free cells to the right.
     */
    auto registerScreens(
        IPEndpoint endpoint,
        const std::vector<ScreenInfo> &screens,
        bool local
    ) -> void;
    auto registerScreens(
        IPEndpoint endpoint,
        std::string_view ownerId,
        const std::vector<ScreenInfo> &screens,
        bool local
    ) -> void;

    /**
     * @brief Remove all screens owned by @p endpoint.
     *
     * @param activeScreen Active screen pointer from the input router, or null.
     * @return true if @p activeScreen was among the removed screens (caller must
     *         clear active state; the pointer is dangling after return).
     */
    auto removeScreen(IPEndpoint endpoint, VirtualScreen *activeScreen) -> bool;

    auto findScreen(const ScreenKey &key) -> VirtualScreen *;
    auto findScreen(const ScreenKey &key) const -> const VirtualScreen *;

    /**
     * @brief Prefer primary local screen; otherwise first local in map order.
     */
    auto firstLocalScreen() -> VirtualScreen *;

    auto rememberOwner(IPEndpoint endpoint, std::string ownerId) -> void;
    auto forgetOwner(IPEndpoint endpoint) -> void;
    auto defaultOwnerId(IPEndpoint endpoint, bool local) const -> std::string;
    auto ownerIdFromEndpoint(IPEndpoint endpoint) const -> std::string;

private:
    auto addScreen(
        IPEndpoint endpoint,
        ScreenKey key,
        GridPosition cell,
        ScreenInfo info,
        bool local
    ) -> VirtualScreen *;
    auto configuredCell(const ScreenKey &key) const -> std::optional<GridPosition>;
    auto rememberScreenLayout(const ScreenKey &key, GridPosition cell) -> void;
    auto saveConfigIfNeeded() -> void;
    auto nextFreeCell(int32_t startX) const -> GridPosition;

    AppConfig mConfig;
    std::filesystem::path mConfigPath;
    // One endpoint may own multiple screens (multi-monitor client).
    std::multimap<IPEndpoint, VirtualScreen> mScreens;
    ScreenTopology mTopology;
    // endpoint → last Hello machineId / owner string for re-registration.
    std::map<IPEndpoint, std::string> mEndpointOwners;
};

MKS_END
