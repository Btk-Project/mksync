#if defined(__linux__)

    #include "preinclude.hpp"

    #include <wayland-client.h>
    #include <xkbcommon/xkbcommon.h>

    #include "virtual-keyboard-unstable-v1.h"
    #include "wlr-virtual-pointer-unstable-v1.h"
    #include "xdg-output-unstable-v1.h"

    #include <poll.h>
    #include <sys/mman.h>
    #include <unistd.h>

    #include <algorithm>
    #include <cerrno>
    #include <chrono>
    #include <cmath>
    #include <cstdint>
    #include <cstdlib>
    #include <cstring>
    #include <limits>
    #include <memory>
    #include <optional>
    #include <string>
    #include <string_view>
    #include <system_error>
    #include <utility>
    #include <vector>

    #include <ilias/net/poller.hpp>
    #include <spdlog/spdlog.h>

    #include "backend.hpp"
    #include "platform.hpp"
    #include "wayland_keymap.hpp"

MKS_BEGIN

class WaylandInputCapture;
class WaylandInputInjector;

namespace
{
    auto makeIoError(std::errc error) -> std::error_code
    {
        return std::make_error_code(error);
    }

    auto systemError() -> std::error_code
    {
        return {errno, std::generic_category()};
    }

    auto monotonicMilliseconds() -> uint32_t
    {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch());
        return static_cast<uint32_t>(elapsed.count());
    }

    template <typename T>
    auto bindGlobal(wl_registry *registry, uint32_t name, uint32_t offered,
                    const wl_interface &interface) -> T *
    {
        const auto version = std::min(offered, static_cast<uint32_t>(interface.version));
        return static_cast<T *>(wl_registry_bind(registry, name, &interface, version));
    }

    struct XkbContextDeleter {
        auto operator()(xkb_context *context) const -> void { xkb_context_unref(context); }
    };

    struct XkbKeymapDeleter {
        auto operator()(xkb_keymap *keymap) const -> void { xkb_keymap_unref(keymap); }
    };

    using XkbContextPtr = std::unique_ptr<xkb_context, XkbContextDeleter>;
    using XkbKeymapPtr  = std::unique_ptr<xkb_keymap, XkbKeymapDeleter>;
} // namespace

class WaylandPlatform final : public Platform,
                              public std::enable_shared_from_this<WaylandPlatform> {
public:
    struct Output {
        WaylandPlatform *platform     = nullptr;
        uint32_t         registryName = 0;
        wl_output       *object       = nullptr;
        zxdg_output_v1  *xdgObject    = nullptr;

        int32_t fallbackX        = 0;
        int32_t fallbackY        = 0;
        int32_t physicalWidthMm  = 0;
        int32_t physicalHeightMm = 0;
        int32_t modeWidth        = 0;
        int32_t modeHeight       = 0;
        int32_t scale            = 1;
        int32_t transform        = WL_OUTPUT_TRANSFORM_NORMAL;

        int32_t logicalX           = 0;
        int32_t logicalY           = 0;
        int32_t logicalWidth       = 0;
        int32_t logicalHeight      = 0;
        bool    hasLogicalPosition = false;
        bool    hasLogicalSize     = false;

        std::string make;
        std::string model;
        std::string name;
        std::string description;
    };

    struct AbsolutePosition {
        uint32_t x      = 0;
        uint32_t y      = 0;
        uint32_t width  = 1;
        uint32_t height = 1;
    };

    explicit WaylandPlatform(bool reportLimitations = true)
    {
        try {
            mDisplay = wl_display_connect(nullptr);
            if (!mDisplay) {
                throw std::system_error(systemError(), "Failed to connect to Wayland compositor");
            }

            mRegistry = wl_display_get_registry(mDisplay);
            if (!mRegistry) {
                throw std::runtime_error("Failed to get Wayland registry");
            }
            if (wl_registry_add_listener(mRegistry, &kRegistryListener, this) != 0) {
                throw std::runtime_error("Failed to listen to Wayland registry");
            }

            // First roundtrip discovers globals. xdg-output objects created from
            // those globals need a second roundtrip to receive their logical
            // geometry.
            roundtrip("discover globals");
            attachXdgOutputs();
            roundtrip("read output geometry");

            SPDLOG_INFO("Wayland backend connected display={} outputs={} xdg-output={} "
                        "virtual-pointer={} virtual-keyboard={}",
                        displayName(), mOutputs.size(), mXdgOutputManager != nullptr,
                        mVirtualPointerManager != nullptr, mVirtualKeyboardManager != nullptr);
            if (reportLimitations && (!mVirtualPointerManager || !mVirtualKeyboardManager)) {
                SPDLOG_WARN("The compositor does not expose both wlroots virtual-input "
                            "extensions; "
                            "Wayland input injection will be unavailable");
            }
            if (reportLimitations) {
                SPDLOG_WARN("Pure Wayland has no global input-capture/grab protocol "
                            "suitable for mksync; "
                            "server mode is unavailable on this backend");
            }
        }
        catch (...) {
            cleanup();
            throw;
        }
    }

    ~WaylandPlatform() override { cleanup(); }

    auto screens() const -> std::vector<ScreenInfo> override
    {
        auto result = std::vector<ScreenInfo>{};
        result.reserve(mOutputs.size());
        for (auto index = 0U; index < mOutputs.size(); ++index) {
            result.push_back(screenInfo(*mOutputs[index], index));
        }
        return result;
    }

    auto createCapture() -> InputCapture::Ptr override;
    auto createInjector() -> InputInjector::Ptr override;

    auto display() const -> wl_display * { return mDisplay; }

    auto seat() const -> wl_seat * { return mSeat; }

    auto virtualPointerManager() const -> zwlr_virtual_pointer_manager_v1 *
    {
        return mVirtualPointerManager;
    }

    auto virtualKeyboardManager() const -> zwp_virtual_keyboard_manager_v1 *
    {
        return mVirtualKeyboardManager;
    }

    auto absolutePosition(uint32_t screenIndex, int32_t x, int32_t y) const
        -> std::optional<AbsolutePosition>
    {
        const auto allScreens = screens();
        if (screenIndex >= allScreens.size() || allScreens.empty()) {
            return std::nullopt;
        }

        auto minX = int64_t{std::numeric_limits<int32_t>::max()};
        auto minY = int64_t{std::numeric_limits<int32_t>::max()};
        auto maxX = int64_t{std::numeric_limits<int32_t>::min()};
        auto maxY = int64_t{std::numeric_limits<int32_t>::min()};
        for (const auto &screen : allScreens) {
            minX = std::min(minX, static_cast<int64_t>(screen.x));
            minY = std::min(minY, static_cast<int64_t>(screen.y));
            maxX = std::max(maxX, static_cast<int64_t>(screen.x) + screen.width);
            maxY = std::max(maxY, static_cast<int64_t>(screen.y) + screen.height);
        }

        const auto &screen  = allScreens[screenIndex];
        const auto  localX  = std::clamp(x, 0, std::max(0, screen.width - 1));
        const auto  localY  = std::clamp(y, 0, std::max(0, screen.height - 1));
        const auto  globalX = static_cast<int64_t>(screen.x) + localX - minX;
        const auto  globalY = static_cast<int64_t>(screen.y) + localY - minY;
        const auto  width   = std::clamp<int64_t>(maxX - minX, 1, UINT32_MAX);
        const auto  height  = std::clamp<int64_t>(maxY - minY, 1, UINT32_MAX);

        return AbsolutePosition{
            .x      = static_cast<uint32_t>(std::clamp<int64_t>(globalX, 0, width - 1)),
            .y      = static_cast<uint32_t>(std::clamp<int64_t>(globalY, 0, height - 1)),
            .width  = static_cast<uint32_t>(width),
            .height = static_cast<uint32_t>(height),
        };
    }

private:
    auto cleanup() -> void
    {
        for (auto &output : mOutputs) {
            destroyOutput(*output);
        }
        mOutputs.clear();

        if (mVirtualKeyboardManager) {
            zwp_virtual_keyboard_manager_v1_destroy(mVirtualKeyboardManager);
            mVirtualKeyboardManager = nullptr;
        }
        if (mVirtualPointerManager) {
            zwlr_virtual_pointer_manager_v1_destroy(mVirtualPointerManager);
            mVirtualPointerManager = nullptr;
        }
        if (mXdgOutputManager) {
            zxdg_output_manager_v1_destroy(mXdgOutputManager);
            mXdgOutputManager = nullptr;
        }
        if (mSeat) {
            if (wl_seat_get_version(mSeat) >= WL_SEAT_RELEASE_SINCE_VERSION) {
                wl_seat_release(mSeat);
            }
            else {
                wl_seat_destroy(mSeat);
            }
            mSeat = nullptr;
        }
        if (mRegistry) {
            wl_registry_destroy(mRegistry);
            mRegistry = nullptr;
        }
        if (mDisplay) {
            (void)wl_display_flush(mDisplay);
            wl_display_disconnect(mDisplay);
            mDisplay = nullptr;
        }
    }
    auto displayName() const -> std::string_view
    {
        const auto *name = std::getenv("WAYLAND_DISPLAY");
        return name && *name ? std::string_view{name} : std::string_view{"wayland-0"};
    }

    auto roundtrip(std::string_view operation) -> void
    {
        if (wl_display_roundtrip(mDisplay) < 0) {
            throw std::system_error(displayError(),
                                    std::string{"Wayland failed to "} + std::string{operation});
        }
    }

    auto displayError() const -> std::error_code
    {
        const auto error = wl_display_get_error(mDisplay);
        return error != 0 ? std::error_code{error, std::generic_category()}
                          : makeIoError(std::errc::protocol_error);
    }

    static auto screenInfo(const Output &output, uint32_t index) -> ScreenInfo
    {
        auto width  = output.logicalWidth;
        auto height = output.logicalHeight;
        if (!output.hasLogicalSize) {
            const auto rotated = output.transform == WL_OUTPUT_TRANSFORM_90 ||
                                 output.transform == WL_OUTPUT_TRANSFORM_270 ||
                                 output.transform == WL_OUTPUT_TRANSFORM_FLIPPED_90 ||
                                 output.transform == WL_OUTPUT_TRANSFORM_FLIPPED_270;
            width  = rotated ? output.modeHeight : output.modeWidth;
            height = rotated ? output.modeWidth : output.modeHeight;
            width /= std::max(1, output.scale);
            height /= std::max(1, output.scale);
        }
        width  = std::max(1, width);
        height = std::max(1, height);

        auto name = output.name;
        if (name.empty()) {
            name = output.make;
            if (!output.model.empty()) {
                if (!name.empty()) {
                    name += ' ';
                }
                name += output.model;
            }
        }
        if (name.empty()) {
            name = fmtlib::format("wayland-output-{}", index);
        }

        auto dpi = 72;
        if (output.physicalWidthMm > 0 && output.modeWidth > 0) {
            dpi = static_cast<int32_t>(
                std::lround(static_cast<double>(output.modeWidth) * 25.4 / output.physicalWidthMm));
        }
        return ScreenInfo{
            .x      = output.hasLogicalPosition ? output.logicalX : output.fallbackX,
            .y      = output.hasLogicalPosition ? output.logicalY : output.fallbackY,
            .width  = width,
            .height = height,
            .dpi    = dpi,
            .name   = std::move(name),
            // Wayland has no standard primary-output property. Keep topology
            // startup deterministic by selecting the first advertised output.
            .primary = index == 0,
        };
    }

    auto attachXdgOutput(Output &output) -> void
    {
        if (!mXdgOutputManager || output.xdgObject) {
            return;
        }
        output.xdgObject = zxdg_output_manager_v1_get_xdg_output(mXdgOutputManager, output.object);
        if (!output.xdgObject ||
            zxdg_output_v1_add_listener(output.xdgObject, &kXdgOutputListener, &output) != 0) {
            throw std::runtime_error("Failed to create xdg-output object");
        }
    }

    auto attachXdgOutputs() -> void
    {
        for (auto &output : mOutputs) {
            attachXdgOutput(*output);
        }
    }

    static auto destroyOutput(Output &output) -> void
    {
        if (output.xdgObject) {
            zxdg_output_v1_destroy(output.xdgObject);
            output.xdgObject = nullptr;
        }
        if (output.object) {
            if (wl_output_get_version(output.object) >= WL_OUTPUT_RELEASE_SINCE_VERSION) {
                wl_output_release(output.object);
            }
            else {
                wl_output_destroy(output.object);
            }
            output.object = nullptr;
        }
    }

    static auto registryGlobal(void *data, wl_registry *registry, uint32_t name,
                               const char *interface, uint32_t version) -> void
    {
        auto &self = *static_cast<WaylandPlatform *>(data);
        if (std::strcmp(interface, wl_seat_interface.name) == 0 && !self.mSeat) {
            self.mSeat = bindGlobal<wl_seat>(registry, name, version, wl_seat_interface);
            return;
        }
        if (std::strcmp(interface, wl_output_interface.name) == 0) {
            auto output          = std::make_unique<Output>();
            output->platform     = &self;
            output->registryName = name;
            output->object = bindGlobal<wl_output>(registry, name, version, wl_output_interface);
            if (!output->object ||
                wl_output_add_listener(output->object, &kOutputListener, output.get()) != 0) {
                SPDLOG_ERROR("Failed to bind Wayland output global {}", name);
                return;
            }
            self.mOutputs.push_back(std::move(output));
            return;
        }
        if (std::strcmp(interface, zxdg_output_manager_v1_interface.name) == 0 &&
            !self.mXdgOutputManager) {
            self.mXdgOutputManager = bindGlobal<zxdg_output_manager_v1>(
                registry, name, version, zxdg_output_manager_v1_interface);
            return;
        }
        if (std::strcmp(interface, zwlr_virtual_pointer_manager_v1_interface.name) == 0 &&
            !self.mVirtualPointerManager) {
            self.mVirtualPointerManager = bindGlobal<zwlr_virtual_pointer_manager_v1>(
                registry, name, version, zwlr_virtual_pointer_manager_v1_interface);
            return;
        }
        if (std::strcmp(interface, zwp_virtual_keyboard_manager_v1_interface.name) == 0 &&
            !self.mVirtualKeyboardManager) {
            self.mVirtualKeyboardManager = bindGlobal<zwp_virtual_keyboard_manager_v1>(
                registry, name, version, zwp_virtual_keyboard_manager_v1_interface);
        }
    }

    static auto registryGlobalRemove(void *data, wl_registry *, uint32_t name) -> void
    {
        auto      &self = *static_cast<WaylandPlatform *>(data);
        const auto it   = std::ranges::find(self.mOutputs, name,
                                            [](const auto &output) { return output->registryName; });
        if (it == self.mOutputs.end()) {
            return;
        }
        destroyOutput(**it);
        self.mOutputs.erase(it);
        SPDLOG_INFO("Wayland output global {} was removed", name);
    }

    static auto outputGeometry(void *data, wl_output *, int32_t x, int32_t y, int32_t physicalWidth,
                               int32_t physicalHeight, int32_t, const char *make, const char *model,
                               int32_t transform) -> void
    {
        auto &output            = *static_cast<Output *>(data);
        output.fallbackX        = x;
        output.fallbackY        = y;
        output.physicalWidthMm  = physicalWidth;
        output.physicalHeightMm = physicalHeight;
        output.make             = make ? make : "";
        output.model            = model ? model : "";
        output.transform        = transform;
    }

    static auto outputMode(void *data, wl_output *, uint32_t flags, int32_t width, int32_t height,
                           int32_t) -> void
    {
        if ((flags & WL_OUTPUT_MODE_CURRENT) == 0) {
            return;
        }
        auto &output      = *static_cast<Output *>(data);
        output.modeWidth  = width;
        output.modeHeight = height;
    }

    static auto outputDone(void *, wl_output *) -> void {}

    static auto outputScale(void *data, wl_output *, int32_t scale) -> void
    {
        static_cast<Output *>(data)->scale = std::max(1, scale);
    }

    static auto outputName(void *data, wl_output *, const char *name) -> void
    {
        static_cast<Output *>(data)->name = name ? name : "";
    }

    static auto outputDescription(void *data, wl_output *, const char *description) -> void
    {
        static_cast<Output *>(data)->description = description ? description : "";
    }

    static auto xdgLogicalPosition(void *data, zxdg_output_v1 *, int32_t x, int32_t y) -> void
    {
        auto &output              = *static_cast<Output *>(data);
        output.logicalX           = x;
        output.logicalY           = y;
        output.hasLogicalPosition = true;
    }

    static auto xdgLogicalSize(void *data, zxdg_output_v1 *, int32_t width, int32_t height) -> void
    {
        auto &output          = *static_cast<Output *>(data);
        output.logicalWidth   = width;
        output.logicalHeight  = height;
        output.hasLogicalSize = width > 0 && height > 0;
    }

    static auto xdgDone(void *, zxdg_output_v1 *) -> void {}

    static auto xdgName(void *data, zxdg_output_v1 *, const char *name) -> void
    {
        auto &output = *static_cast<Output *>(data);
        if (output.name.empty()) {
            output.name = name ? name : "";
        }
    }

    static auto xdgDescription(void *data, zxdg_output_v1 *, const char *description) -> void
    {
        auto &output = *static_cast<Output *>(data);
        if (output.description.empty()) {
            output.description = description ? description : "";
        }
    }

    static constexpr wl_registry_listener kRegistryListener{
        .global        = registryGlobal,
        .global_remove = registryGlobalRemove,
    };
    static constexpr wl_output_listener kOutputListener{
        .geometry    = outputGeometry,
        .mode        = outputMode,
        .done        = outputDone,
        .scale       = outputScale,
        .name        = outputName,
        .description = outputDescription,
    };
    static constexpr zxdg_output_v1_listener kXdgOutputListener{
        .logical_position = xdgLogicalPosition,
        .logical_size     = xdgLogicalSize,
        .done             = xdgDone,
        .name             = xdgName,
        .description      = xdgDescription,
    };

    wl_display                          *mDisplay                = nullptr;
    wl_registry                         *mRegistry               = nullptr;
    wl_seat                             *mSeat                   = nullptr;
    zxdg_output_manager_v1              *mXdgOutputManager       = nullptr;
    zwlr_virtual_pointer_manager_v1     *mVirtualPointerManager  = nullptr;
    zwp_virtual_keyboard_manager_v1     *mVirtualKeyboardManager = nullptr;
    std::vector<std::unique_ptr<Output>> mOutputs;
    std::weak_ptr<WaylandInputCapture>   mInputCapture;
    std::weak_ptr<WaylandInputInjector>  mInputInjector;
};

class WaylandInputCapture final : public InputCapture {
public:
    explicit WaylandInputCapture(std::shared_ptr<WaylandPlatform> platform)
        : mPlatform(std::move(platform))
    {
    }

    auto initialize() -> IoTask<void> override
    {
        SPDLOG_DEBUG("Wayland global input capture is unsupported without XDG Portal/libei");
        co_return Err(makeIoError(std::errc::operation_not_supported));
    }

    auto shutdown() -> Task<void> override { co_return; }

    auto nextEvent() -> Task<InputEvent> override
    {
        throw std::runtime_error("Wayland input capture is unavailable");
        co_return InputEvent{};
    }

    auto setRemoteControlActive(bool) -> IoResult<void> override
    {
        return Err(makeIoError(std::errc::operation_not_supported));
    }

    auto moveLocalCursor(uint32_t, int32_t, int32_t) -> IoResult<void> override
    {
        return Err(makeIoError(std::errc::operation_not_supported));
    }

private:
    std::shared_ptr<WaylandPlatform> mPlatform;
};

class WaylandInputInjector final : public InputInjector {
public:
    explicit WaylandInputInjector(std::shared_ptr<WaylandPlatform> platform)
        : mPlatform(std::move(platform))
    {
    }

    ~WaylandInputInjector() override { close(); }

    auto initialize() -> IoTask<void> override
    {
        close();

        if (!mPlatform->seat() || !mPlatform->virtualPointerManager() ||
            !mPlatform->virtualKeyboardManager()) {
            SPDLOG_DEBUG("Wayland injection requires wl_seat, "
                         "zwlr_virtual_pointer_manager_v1 and "
                         "zwp_virtual_keyboard_manager_v1");
            co_return Err(makeIoError(std::errc::operation_not_supported));
        }

        mVirtualPointer = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(
            mPlatform->virtualPointerManager(), mPlatform->seat());
        mVirtualKeyboard = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(
            mPlatform->virtualKeyboardManager(), mPlatform->seat());
        if (!mVirtualPointer || !mVirtualKeyboard) {
            close();
            co_return Err(makeIoError(std::errc::not_enough_memory));
        }

        auto keymap = createKeymap();
        if (!keymap) {
            const auto error = keymap.error();
            close();
            co_return Err(error);
        }

        auto poller = co_await ilias::Poller::make(wl_display_get_fd(mPlatform->display()),
                                                   ilias::IoDescriptor::Socket);
        if (!poller) {
            const auto error = poller.error();
            close();
            co_return Err(error);
        }
        mPoller      = std::move(*poller);
        auto flushed = co_await flush();
        if (!flushed) {
            const auto error = flushed.error();
            close();
            co_return Err(error);
        }

        SPDLOG_INFO("Wayland wlroots virtual-pointer/virtual-keyboard injection started");
        co_return {};
    }

    auto shutdown() -> Task<void> override
    {
        close();
        co_return;
    }

    auto inject(const InputEvent &event) -> IoTask<void> override
    {
        if (!mVirtualPointer || !mVirtualKeyboard || !mPoller) {
            co_return Err(makeIoError(std::errc::not_connected));
        }

        auto error = std::error_code{};
        std::visit([&](const auto &value) { error = injectOne(value); }, event);
        if (error) {
            SPDLOG_WARN("Wayland failed to queue input event {}: {}", event, error.message());
            co_return Err(error);
        }
        ILIAS_CO_TRYV(co_await flush());
        SPDLOG_TRACE("Wayland injected input event {}", event);
        co_return {};
    }

private:
    auto close() -> void
    {
        if (mPoller) {
            auto _ = mPoller.cancel();
            mPoller.close();
        }
        if (mVirtualKeyboard) {
            zwp_virtual_keyboard_v1_destroy(mVirtualKeyboard);
            mVirtualKeyboard = nullptr;
        }
        if (mVirtualPointer) {
            zwlr_virtual_pointer_v1_destroy(mVirtualPointer);
            mVirtualPointer = nullptr;
        }
        mKeymap.reset();
        mXkbContext.reset();
    }

    auto flush() -> IoTask<void>
    {
        while (true) {
            if (wl_display_flush(mPlatform->display()) >= 0) {
                co_return {};
            }
            if (errno != EAGAIN) {
                co_return Err(systemError());
            }

            ILIAS_CO_TRY(auto events, co_await mPoller.poll(POLLOUT));
            if ((events & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
                co_return Err(makeIoError(std::errc::connection_reset));
            }
        }
    }

    auto createKeymap() -> IoResult<void>
    {
        mXkbContext.reset(xkb_context_new(XKB_CONTEXT_NO_FLAGS));
        if (!mXkbContext) {
            return Err(makeIoError(std::errc::not_enough_memory));
        }
        mKeymap.reset(
            xkb_keymap_new_from_names(mXkbContext.get(), nullptr, XKB_KEYMAP_COMPILE_NO_FLAGS));
        if (!mKeymap) {
            return Err(makeIoError(std::errc::invalid_argument));
        }

        auto *rawText = xkb_keymap_get_as_string(mKeymap.get(), XKB_KEYMAP_FORMAT_TEXT_V1);
        if (!rawText) {
            return Err(makeIoError(std::errc::not_enough_memory));
        }
        auto       text = std::unique_ptr<char, decltype(&std::free)>{rawText, &std::free};
        const auto size = std::strlen(text.get()) + 1;
        if (size > std::numeric_limits<uint32_t>::max()) {
            return Err(makeIoError(std::errc::value_too_large));
        }

        const auto fd = ::memfd_create("mksync-wayland-keymap", MFD_CLOEXEC);
        if (fd < 0) {
            return Err(systemError());
        }
        auto file    = std::unique_ptr<int, void (*)(int *)>{new int{fd}, [](int *owned) {
                                                              ::close(*owned);
                                                              delete owned;
                                                          }};
        auto written = size_t{0};
        while (written < size) {
            const auto count = ::write(fd, text.get() + written, size - written);
            if (count > 0) {
                written += static_cast<size_t>(count);
                continue;
            }
            if (count < 0 && errno == EINTR) {
                continue;
            }
            return Err(count < 0 ? systemError() : makeIoError(std::errc::io_error));
        }
        if (::lseek(fd, 0, SEEK_SET) < 0) {
            return Err(systemError());
        }

        zwp_virtual_keyboard_v1_keymap(mVirtualKeyboard, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd,
                                       static_cast<uint32_t>(size));
        mShiftMask = modifierMask(XKB_MOD_NAME_SHIFT);
        mCtrlMask  = modifierMask(XKB_MOD_NAME_CTRL);
        mAltMask   = modifierMask(XKB_MOD_NAME_ALT);
        mLogoMask  = modifierMask(XKB_MOD_NAME_LOGO);
        return {};
    }

    auto modifierMask(const char *name) const -> uint32_t
    {
        const auto index = xkb_keymap_mod_get_index(mKeymap.get(), name);
        if (index == XKB_MOD_INVALID || index >= 32) {
            return 0;
        }
        return uint32_t{1} << index;
    }

    auto modifiersFor(KeyModifier modifiers) const -> uint32_t
    {
        auto mask = uint32_t{0};
        if ((modifiers & (KeyModifier::LeftShift | KeyModifier::RightShift)) != KeyModifier::None) {
            mask |= mShiftMask;
        }
        if ((modifiers & (KeyModifier::LeftCtrl | KeyModifier::RightCtrl)) != KeyModifier::None) {
            mask |= mCtrlMask;
        }
        if ((modifiers & (KeyModifier::LeftAlt | KeyModifier::RightAlt)) != KeyModifier::None) {
            mask |= mAltMask;
        }
        if ((modifiers & (KeyModifier::LeftMeta | KeyModifier::RightMeta)) != KeyModifier::None) {
            mask |= mLogoMask;
        }
        return mask;
    }

    auto queuePointerPosition(uint32_t screenIndex, int32_t x, int32_t y) -> std::error_code
    {
        const auto position = mPlatform->absolutePosition(screenIndex, x, y);
        if (!position) {
            return makeIoError(std::errc::invalid_argument);
        }
        zwlr_virtual_pointer_v1_motion_absolute(mVirtualPointer, monotonicMilliseconds(),
                                                position->x, position->y, position->width,
                                                position->height);
        return {};
    }

    static auto pointerButton(MouseButton button) -> std::optional<uint32_t>
    {
        switch (button) {
        case MouseButton::Left:
            return BTN_LEFT;
        case MouseButton::Right:
            return BTN_RIGHT;
        case MouseButton::Middle:
            return BTN_MIDDLE;
        case MouseButton::None:
            return std::nullopt;
        }
        return std::nullopt;
    }

    auto injectOne(const MouseMoveEvent &event) -> std::error_code
    {
        if (auto error = queuePointerPosition(event.screenIndex, event.x, event.y); error) {
            return error;
        }
        zwlr_virtual_pointer_v1_frame(mVirtualPointer);
        return {};
    }

    auto injectOne(const MouseButtonEvent &event) -> std::error_code
    {
        if (auto error = queuePointerPosition(event.screenIndex, event.x, event.y); error) {
            return error;
        }
        const auto button = pointerButton(event.button);
        if (!button) {
            return makeIoError(std::errc::invalid_argument);
        }
        zwlr_virtual_pointer_v1_button(mVirtualPointer, monotonicMilliseconds(), *button,
                                       event.release ? WL_POINTER_BUTTON_STATE_RELEASED
                                                     : WL_POINTER_BUTTON_STATE_PRESSED);
        zwlr_virtual_pointer_v1_frame(mVirtualPointer);
        return {};
    }

    auto injectOne(const MouseWheelEvent &event) -> std::error_code
    {
        const auto time = monotonicMilliseconds();
        if (event.deltaY != 0) {
            const auto steps = -event.deltaY / 120;
            const auto value = wl_fixed_from_double(-static_cast<double>(event.deltaY) / 12.0);
            if (steps != 0) {
                zwlr_virtual_pointer_v1_axis_discrete(
                    mVirtualPointer, time, WL_POINTER_AXIS_VERTICAL_SCROLL, value, steps);
            }
            else {
                zwlr_virtual_pointer_v1_axis(mVirtualPointer, time, WL_POINTER_AXIS_VERTICAL_SCROLL,
                                             value);
            }
        }
        if (event.deltaX != 0) {
            const auto steps = event.deltaX / 120;
            const auto value = wl_fixed_from_double(static_cast<double>(event.deltaX) / 12.0);
            if (steps != 0) {
                zwlr_virtual_pointer_v1_axis_discrete(
                    mVirtualPointer, time, WL_POINTER_AXIS_HORIZONTAL_SCROLL, value, steps);
            }
            else {
                zwlr_virtual_pointer_v1_axis(mVirtualPointer, time,
                                             WL_POINTER_AXIS_HORIZONTAL_SCROLL, value);
            }
        }
        zwlr_virtual_pointer_v1_frame(mVirtualPointer);
        return {};
    }

    auto injectOne(const KeyEvent &event) -> std::error_code
    {
        const auto keyCode = wayland::evdevKeyCode(event.key);
        if (!keyCode) {
            return makeIoError(std::errc::invalid_argument);
        }
        zwp_virtual_keyboard_v1_key(mVirtualKeyboard, monotonicMilliseconds(), *keyCode,
                                    event.release ? WL_KEYBOARD_KEY_STATE_RELEASED
                                                  : WL_KEYBOARD_KEY_STATE_PRESSED);
        zwp_virtual_keyboard_v1_modifiers(mVirtualKeyboard, modifiersFor(event.modifiers), 0, 0, 0);
        return {};
    }

    std::shared_ptr<WaylandPlatform> mPlatform;
    zwlr_virtual_pointer_v1         *mVirtualPointer  = nullptr;
    zwp_virtual_keyboard_v1         *mVirtualKeyboard = nullptr;
    ilias::Poller                    mPoller;
    XkbContextPtr                    mXkbContext;
    XkbKeymapPtr                     mKeymap;
    uint32_t                         mShiftMask = 0;
    uint32_t                         mCtrlMask  = 0;
    uint32_t                         mAltMask   = 0;
    uint32_t                         mLogoMask  = 0;
};

auto WaylandPlatform::createCapture() -> InputCapture::Ptr
{
    if (!mInputCapture.expired()) {
        throw std::runtime_error("InputCapture already created");
    }
    auto capture  = std::make_shared<WaylandInputCapture>(shared_from_this());
    mInputCapture = capture;
    return capture;
}

auto WaylandPlatform::createInjector() -> InputInjector::Ptr
{
    if (!mInputInjector.expired()) {
        throw std::runtime_error("InputInjector already created");
    }
    auto injector  = std::make_shared<WaylandInputInjector>(shared_from_this());
    mInputInjector = injector;
    return injector;
}

auto createWaylandPlatform() -> Platform::Ptr
{
    try {
        return std::make_shared<WaylandPlatform>();
    }
    catch (const std::exception &error) {
        SPDLOG_ERROR("Failed to create Wayland platform: {}", error.what());
        return nullptr;
    }
}

namespace
{
    auto createWaylandPlatformForCheck() -> Platform::Ptr
    {
        try {
            return std::make_shared<WaylandPlatform>(false);
        }
        catch (const std::exception &error) {
            SPDLOG_DEBUG("Wayland backend check could not create platform: {}", error.what());
            return nullptr;
        }
    }

    auto checkWaylandBackend() -> Task<BackendCheck>
    {
        co_return co_await probeBackend("Wayland wlroots protocols", createWaylandPlatformForCheck);
    }

    const BackendRegistration kWaylandBackendRegistration{
        BackendDescriptor{
                          .name        = "wayland-wlr",
                          .displayName = "Wayland (wlroots protocols)",
                          .order       = 100,
                          .check       = checkWaylandBackend,
                          .create      = createWaylandPlatform,
                          }
    };
} // namespace

MKS_END

#endif
