#include "backend.hpp"

#include <algorithm>
#include <mutex>
#include <system_error>
#include <utility>

#include <spdlog/spdlog.h>

MKS_BEGIN

namespace
{
    auto registryMutex() -> std::mutex &
    {
        static auto mutex = std::mutex{};
        return mutex;
    }

    auto registry() -> std::vector<BackendDescriptor> &
    {
        static auto descriptors = std::vector<BackendDescriptor>{};
        return descriptors;
    }

    auto hasRequirement(BackendRequirement value, BackendRequirement required) -> bool
    {
        return (value & required) != BackendRequirement::None;
    }

    auto unavailableCheck(std::string detail) -> BackendCheck
    {
        return BackendCheck{
            .available = false,
            .detail    = std::move(detail),
            .screens   = {.supported = false, .detail = "Backend unavailable"},
            .capture   = {.supported = false, .detail = "Backend unavailable"},
            .injection = {.supported = false, .detail = "Backend unavailable"},
        };
    }

    auto findBackend(std::string_view name) -> std::optional<BackendDescriptor>
    {
        const auto descriptors = registeredBackends();
        const auto it          = std::ranges::find(descriptors, name, &BackendDescriptor::name);
        if (it == descriptors.end()) {
            return std::nullopt;
        }
        return *it;
    }
} // namespace

auto BackendCheck::supports(BackendRequirement requirements) const -> bool
{
    if (!available) {
        return false;
    }
    if (hasRequirement(requirements, BackendRequirement::Screens) && !screens.supported) {
        return false;
    }
    if (hasRequirement(requirements, BackendRequirement::Capture) && !capture.supported) {
        return false;
    }
    if (hasRequirement(requirements, BackendRequirement::Injection) && !injection.supported) {
        return false;
    }
    return true;
}

BackendRegistration::BackendRegistration(BackendDescriptor descriptor)
{
    if (descriptor.name.empty() || descriptor.displayName.empty() || descriptor.check == nullptr ||
        descriptor.create == nullptr) {
        SPDLOG_ERROR("Ignoring incomplete platform backend registration");
        return;
    }

    auto       lock      = std::scoped_lock{registryMutex()};
    const auto duplicate = std::ranges::find(registry(), descriptor.name, &BackendDescriptor::name);
    if (duplicate != registry().end()) {
        SPDLOG_ERROR("Ignoring duplicate platform backend '{}'", descriptor.name);
        return;
    }
    registry().push_back(descriptor);
}

auto registeredBackends() -> std::vector<BackendDescriptor>
{
    auto lock   = std::scoped_lock{registryMutex()};
    auto result = registry();
    std::ranges::sort(result, {}, [](const BackendDescriptor &descriptor) {
        return std::pair{descriptor.order, descriptor.name};
    });
    return result;
}

auto checkBackend(std::string_view name) -> Task<BackendCheck>
{
    const auto descriptor = findBackend(name);
    if (!descriptor) {
        co_return unavailableCheck(fmtlib::format("Unknown backend '{}'", name));
    }
    try {
        co_return co_await descriptor->check();
    }
    catch (const std::exception &error) {
        co_return unavailableCheck(error.what());
    }
}

auto checkBackends() -> Task<std::vector<CheckedBackend>>
{
    auto result = std::vector<CheckedBackend>{};
    for (const auto &descriptor : registeredBackends()) {
        result.push_back(CheckedBackend{
            .descriptor = descriptor,
            .check      = co_await checkBackend(descriptor.name),
        });
    }
    co_return result;
}

auto selectBackend(std::string_view name, BackendRequirement requirements)
    -> IoTask<PlatformSelection>
{
    auto candidates = std::vector<BackendDescriptor>{};
    if (name.empty() || name == "auto") {
        candidates = registeredBackends();
    }
    else if (const auto descriptor = findBackend(name)) {
        candidates.push_back(*descriptor);
    }
    else {
        SPDLOG_ERROR("Unknown platform backend '{}'", name);
        co_return Err(std::make_error_code(std::errc::no_such_device));
    }

    for (const auto &descriptor : candidates) {
        auto check = co_await checkBackend(descriptor.name);
        if (!check.supports(requirements)) {
            SPDLOG_INFO("Platform backend '{}' is not suitable: {}", descriptor.name, check.detail);
            continue;
        }
        auto platform = descriptor.create();
        if (!platform) {
            SPDLOG_WARN("Platform backend '{}' passed check but creation failed", descriptor.name);
            continue;
        }
        SPDLOG_INFO("Selected platform backend '{}' ({})", descriptor.name, descriptor.displayName);
        co_return PlatformSelection{
            .platform   = std::move(platform),
            .descriptor = descriptor,
            .check      = std::move(check),
        };
    }

    co_return Err(std::make_error_code(std::errc::operation_not_supported));
}

auto probeBackend(std::string_view displayName, BackendDescriptor::CreateFn create)
    -> Task<BackendCheck>
{
    auto          result = BackendCheck{};
    Platform::Ptr platform;
    try {
        platform = create();
    }
    catch (const std::exception &error) {
        co_return unavailableCheck(error.what());
    }
    if (!platform) {
        co_return unavailableCheck(fmtlib::format("{} could not be created", displayName));
    }

    result.available = true;
    result.detail    = fmtlib::format("{} is available", displayName);
    try {
        const auto screens       = platform->screens();
        result.screens.supported = !screens.empty();
        result.screens.detail    = screens.empty() ? "No outputs were reported"
                                                   : fmtlib::format("{} output(s)", screens.size());
    }
    catch (const std::exception &error) {
        result.screens.detail = error.what();
    }

    try {
        auto capture = platform->createCapture();
        if (!capture) {
            result.capture.detail = "Capture interface is not provided";
        }
        else {
            auto initialized         = co_await capture->initialize();
            result.capture.supported = initialized.has_value();
            result.capture.detail =
                initialized ? "Capture initialization succeeded" : initialized.error().message();
            co_await capture->shutdown();
        }
    }
    catch (const std::exception &error) {
        result.capture.detail = error.what();
    }

    try {
        auto injector = platform->createInjector();
        if (!injector) {
            result.injection.detail = "Injection interface is not provided";
        }
        else {
            auto initialized           = co_await injector->initialize();
            result.injection.supported = initialized.has_value();
            result.injection.detail =
                initialized ? "Injection initialization succeeded" : initialized.error().message();
            co_await injector->shutdown();
        }
    }
    catch (const std::exception &error) {
        result.injection.detail = error.what();
    }

    co_return result;
}

MKS_END
