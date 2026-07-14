#pragma once

#include <memory>
#include <string_view>

#include <ilias/io/error.hpp>
#include <xcb/xcb.h>

namespace mks
{

    // Owns exactly one XCB connection. No Xlib Display may be created from or used with this
    // connection: the owner is also the sole consumer of its reply and event queues.
    class XcbConnection final {
    public:
        static auto connect(std::string_view displayName)
            -> ilias::IoResult<std::unique_ptr<XcbConnection>>;

        XcbConnection(const XcbConnection &)                     = delete;
        auto operator=(const XcbConnection &) -> XcbConnection & = delete;
        XcbConnection(XcbConnection &&)                          = delete;
        auto operator=(XcbConnection &&) -> XcbConnection &      = delete;

        ~XcbConnection();

        auto get() const -> xcb_connection_t *;
        auto defaultScreen() const -> int;
        auto fileDescriptor() const -> int;
        auto screen(int index) const -> const xcb_screen_t *;

        auto check(xcb_void_cookie_t cookie) const -> ilias::IoResult<void>;
        auto flush() const -> ilias::IoResult<void>;

    private:
        XcbConnection(xcb_connection_t *connection, int defaultScreen);

        xcb_connection_t *mConnection    = nullptr;
        int               mDefaultScreen = 0;
    };

} // namespace mks
