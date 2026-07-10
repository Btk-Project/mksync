#include "server_screens.hpp"

#include <algorithm>

MKS_BEGIN

// MARK: Construction / queries

ServerScreenStore::ServerScreenStore(AppConfig config, std::filesystem::path configPath)
    : mConfig(std::move(config)),
      mConfigPath(std::move(configPath)) {
}

auto ServerScreenStore::config() const -> const AppConfig & {
    return mConfig;
}

auto ServerScreenStore::topologyScreens() const -> std::vector<TopologyScreen> {
    return mTopology.screens();
}

auto ServerScreenStore::topology() const -> const ScreenTopology & {
    return mTopology;
}

// MARK: Register / remove

auto ServerScreenStore::registerScreens(
    IPEndpoint endpoint,
    const std::vector<ScreenInfo> &screens,
    bool local
) -> void {
    registerScreens(endpoint, defaultOwnerId(endpoint, local), screens, local);
}

auto ServerScreenStore::registerScreens(
    IPEndpoint endpoint,
    std::string_view ownerId,
    const std::vector<ScreenInfo> &screens,
    bool local
) -> void {
    auto primaryIndex = 0U;
    for (auto index = 0U; index < screens.size(); ++index) {
        if (screens[index].primary) {
            primaryIndex = index;
            break;
        }
    }

    auto primaryRegistered = false;
    for (auto index = 0U; index < screens.size(); ++index) {
        const auto &info = screens[index];
        auto key = ScreenKey {
            .ownerId = std::string {ownerId},
            .screenIndex = index,
        };
        auto cell = GridPosition {};
        auto usedPersistedCell = false;
        if (auto configured = configuredCell(key)) {
            // Persisted layout wins over auto placement so machineId-based
            // screen identity remains stable across reconnects and restarts.
            cell = *configured;
            usedPersistedCell = true;
            if (local && (info.primary || index == primaryIndex)) {
                primaryRegistered = true;
            }
        }
        else if (local && info.primary && !primaryRegistered) {
            // The local primary is the anchor for the default topology.
            cell = {0, 0};
            primaryRegistered = true;
        }
        else if (local && index == primaryIndex && !primaryRegistered) {
            // Some platforms may not mark a primary screen. Keep the first
            // reported local screen as the anchor in that case.
            cell = {0, 0};
            primaryRegistered = true;
        }
        else {
            // Temporary fallback for first run: pack new screens to the right.
            // Formal layout editing should write explicit cells into config.
            cell = nextFreeCell(1);
        }

        // Automatically remembered cells can become stale when the local
        // monitor count changes. Repair by packing to the next free cell.
        const auto cellOccupied = std::ranges::any_of(mTopology.screens(), [&](const auto &screen) {
            return screen.cell == cell;
        });
        if (usedPersistedCell && cellOccupied) {
            const auto replacement = nextFreeCell(1);
            SPDLOG_WARN(
                "Server layout cell {} for screen {}:{} is occupied; moving it to {}",
                cell,
                key.ownerId,
                key.screenIndex,
                replacement
            );
            cell = replacement;
        }

        addScreen(endpoint, std::move(key), cell, info, local);
    }

    SPDLOG_INFO("Server topology screens {}", mTopology.screens());
}

auto ServerScreenStore::addScreen(
    IPEndpoint endpoint,
    ScreenKey key,
    GridPosition cell,
    ScreenInfo info,
    bool local
) -> VirtualScreen * {
    // Register in topology first: if the cell/key is invalid, do not create a
    // routeable VirtualScreen that can later receive input messages.
    auto topologyResult = mTopology.addScreen(TopologyScreen {
        .key = key,
        .cell = cell,
        .info = info,
        .local = local,
    });
    if (!topologyResult) {
        SPDLOG_ERROR(
            "Server failed to register screen {}:{} in topology: {}",
            key.ownerId,
            key.screenIndex,
            topologyResult.error().message()
        );
        return nullptr;
    }

    auto it = mScreens.emplace(std::pair {endpoint, VirtualScreen {
        .endpoint = endpoint,
        .key = std::move(key),
        .cell = cell,
        .local = local,
        .info = info,
    }});
    // Only successful registrations are persisted; failed topology mutations
    // would otherwise corrupt the remembered layout.
    rememberScreenLayout(it->second.key, it->second.cell);
    SPDLOG_INFO("Server current screens {}", mScreens);
    return &it->second;
}

auto ServerScreenStore::removeScreen(IPEndpoint endpoint, VirtualScreen *activeScreen) -> bool {
    auto range = mScreens.equal_range(endpoint);
    auto ownerIds = std::vector<std::string> {};
    auto activeRemoved = false;
    for (auto it = range.first; it != range.second; ++it) {
        // One endpoint can own multiple screens. removeOwner works by stable
        // owner id, so collect each id once before erasing mScreens.
        if (std::ranges::find(ownerIds, it->second.key.ownerId) == ownerIds.end()) {
            ownerIds.push_back(it->second.key.ownerId);
        }
        if (activeScreen != nullptr && &it->second == activeScreen) {
            activeRemoved = true;
        }
    }
    mScreens.erase(endpoint);
    for (const auto &registeredOwnerId : ownerIds) {
        mTopology.removeOwner(registeredOwnerId);
    }
    SPDLOG_INFO("Server current screens {}", mScreens);
    return activeRemoved;
}

// MARK: Lookup / owners

auto ServerScreenStore::findScreen(const ScreenKey &key) -> VirtualScreen * {
    for (auto &[_, screen] : mScreens) {
        if (screen.key == key) {
            return &screen;
        }
    }
    return nullptr;
}

auto ServerScreenStore::findScreen(const ScreenKey &key) const -> const VirtualScreen * {
    for (const auto &[_, screen] : mScreens) {
        if (screen.key == key) {
            return &screen;
        }
    }
    return nullptr;
}

auto ServerScreenStore::firstLocalScreen() -> VirtualScreen * {
    auto selected = mScreens.end();
    for (auto it = mScreens.begin(); it != mScreens.end(); ++it) {
        if (!it->second.local) {
            continue;
        }
        if (selected == mScreens.end() || it->second.info.primary) {
            selected = it;
        }
        if (it->second.info.primary) {
            break;
        }
    }
    if (selected == mScreens.end()) {
        return nullptr;
    }
    return &selected->second;
}

auto ServerScreenStore::rememberOwner(IPEndpoint endpoint, std::string ownerId) -> void {
    mEndpointOwners[endpoint] = std::move(ownerId);
}

auto ServerScreenStore::forgetOwner(IPEndpoint endpoint) -> void {
    mEndpointOwners.erase(endpoint);
}

auto ServerScreenStore::ownerIdFromEndpoint(IPEndpoint endpoint) const -> std::string {
    return fmtlib::format("{}", endpoint);
}

auto ServerScreenStore::defaultOwnerId(IPEndpoint endpoint, bool local) const -> std::string {
    if (local && !mConfig.machineId.empty()) {
        // The server's own screens should also use a stable owner id so a saved
        // layout survives binding to a different local port.
        return mConfig.machineId;
    }

    if (auto it = mEndpointOwners.find(endpoint); it != mEndpointOwners.end() && !it->second.empty()) {
        return it->second;
    }

    return ownerIdFromEndpoint(endpoint);
}

// MARK: Layout persistence / auto placement

auto ServerScreenStore::configuredCell(const ScreenKey &key) const -> std::optional<GridPosition> {
    return findScreenLayout(mConfig, key.ownerId, key.screenIndex);
}

auto ServerScreenStore::rememberScreenLayout(const ScreenKey &key, GridPosition cell) -> void {
    // Persist every registered screen so auto-assigned cells become stable on
    // the next startup. Configured cells are updated in-place by owner+index.
    upsertScreenLayout(mConfig, ScreenLayoutConfig {
        .ownerId = key.ownerId,
        .screenIndex = key.screenIndex,
        .cell = cell,
    });
    saveConfigIfNeeded();
}

auto ServerScreenStore::saveConfigIfNeeded() -> void {
    if (mConfigPath.empty()) {
        return;
    }

    auto saved = saveConfig(mConfigPath, mConfig);
    if (!saved) {
        SPDLOG_WARN(
            "Server failed to save config {}: {}",
            mConfigPath.string(),
            saved.error().message()
        );
    }
}

auto ServerScreenStore::nextFreeCell(int32_t startX) const -> GridPosition {
    auto cell = GridPosition {.x = startX, .y = 0};
    while (true) {
        // Auto layout is deliberately simple for now: scan to the right until
        // a free grid cell exists. Persisted config should replace it later.
        auto occupied = false;
        for (const auto &screen : mTopology.screens()) {
            if (screen.cell == cell) {
                occupied = true;
                break;
            }
        }
        if (!occupied) {
            return cell;
        }
        ++cell.x;
    }
}

MKS_END
