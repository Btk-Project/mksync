#pragma once

#include "platform.hpp"
#include "preinclude.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

MKS_BEGIN

enum class BackendRequirement : uint8_t
{
    None      = 0,
    Screens   = 1U << 0U,
    Capture   = 1U << 1U,
    Injection = 1U << 2U,
};

constexpr auto operator|(BackendRequirement left, BackendRequirement right) -> BackendRequirement
{
    return static_cast<BackendRequirement>(static_cast<uint8_t>(left) |
                                           static_cast<uint8_t>(right));
}

constexpr auto operator&(BackendRequirement left, BackendRequirement right) -> BackendRequirement
{
    return static_cast<BackendRequirement>(static_cast<uint8_t>(left) &
                                           static_cast<uint8_t>(right));
}

struct BackendFeatureCheck {
    bool        supported = false;
    std::string detail;
};

struct BackendCheck {
    bool                available = false;
    std::string         detail;
    BackendFeatureCheck screens;
    BackendFeatureCheck capture;
    BackendFeatureCheck injection;

    auto supports(BackendRequirement requirements) const -> bool;
};

struct BackendDescriptor {
    using CheckFn  = auto (*)() -> Task<BackendCheck>;
    using CreateFn = auto (*)() -> Platform::Ptr;

    std::string_view name;
    std::string_view displayName;
    uint32_t         order  = 0;
    CheckFn          check  = nullptr;
    CreateFn         create = nullptr;
};

struct CheckedBackend {
    BackendDescriptor descriptor;
    BackendCheck      check;
};

struct PlatformSelection {
    Platform::Ptr     platform;
    BackendDescriptor descriptor;
    BackendCheck      check;
};

class BackendRegistration {
public:
    explicit BackendRegistration(BackendDescriptor descriptor);
};

auto registeredBackends() -> std::vector<BackendDescriptor>;
auto checkBackend(std::string_view name) -> Task<BackendCheck>;
auto checkBackends() -> Task<std::vector<CheckedBackend>>;
auto selectBackend(std::string_view name, BackendRequirement requirements)
    -> IoTask<PlatformSelection>;

// Shared implementation for backends whose check consists of constructing the
// backend and initializing each public interface. A backend may wrap or replace
// this to apply stricter rules.
auto probeBackend(std::string_view displayName, BackendDescriptor::CreateFn create)
    -> Task<BackendCheck>;

MKS_END
