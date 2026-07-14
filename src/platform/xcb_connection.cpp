#include "preinclude.hpp"

#include "xcb_connection.hpp"

#include <cstdlib>
#include <string>
#include <system_error>

namespace mks
{

    namespace
    {

        auto connectionError(int error) -> std::error_code
        {
            switch (error) {
            case XCB_CONN_ERROR:
                return std::make_error_code(std::errc::connection_refused);
            case XCB_CONN_CLOSED_EXT_NOTSUPPORTED:
                return std::make_error_code(std::errc::protocol_not_supported);
            case XCB_CONN_CLOSED_MEM_INSUFFICIENT:
                return std::make_error_code(std::errc::not_enough_memory);
            case XCB_CONN_CLOSED_REQ_LEN_EXCEED:
                return std::make_error_code(std::errc::message_size);
            case XCB_CONN_CLOSED_PARSE_ERR:
                return std::make_error_code(std::errc::invalid_argument);
            case XCB_CONN_CLOSED_INVALID_SCREEN:
                return std::make_error_code(std::errc::no_such_device);
            case XCB_CONN_CLOSED_FDPASSING_FAILED:
            default:
                return std::make_error_code(std::errc::io_error);
            }
        }

    } // namespace

    auto XcbConnection::connect(std::string_view displayName)
        -> ilias::IoResult<std::unique_ptr<XcbConnection>>
    {
        auto        screen           = 0;
        const auto  ownedDisplayName = std::string{displayName};
        const auto *name       = ownedDisplayName.empty() ? nullptr : ownedDisplayName.c_str();
        auto       *connection = xcb_connect(name, &screen);
        if (!connection) {
            return ilias::Err(std::make_error_code(std::errc::not_enough_memory));
        }

        const auto error = xcb_connection_has_error(connection);
        if (error != 0) {
            xcb_disconnect(connection);
            return ilias::Err(connectionError(error));
        }
        return std::unique_ptr<XcbConnection>{
            new XcbConnection{connection, screen}
        };
    }

    XcbConnection::XcbConnection(xcb_connection_t *connection, int defaultScreen)
        : mConnection(connection), mDefaultScreen(defaultScreen)
    {
    }

    XcbConnection::~XcbConnection()
    {
        if (mConnection) {
            xcb_disconnect(mConnection);
        }
    }

    auto XcbConnection::get() const -> xcb_connection_t *
    {
        return mConnection;
    }

    auto XcbConnection::defaultScreen() const -> int
    {
        return mDefaultScreen;
    }

    auto XcbConnection::fileDescriptor() const -> int
    {
        return xcb_get_file_descriptor(mConnection);
    }

    auto XcbConnection::screen(int index) const -> const xcb_screen_t *
    {
        if (index < 0) {
            return nullptr;
        }
        auto iterator = xcb_setup_roots_iterator(xcb_get_setup(mConnection));
        for (auto current = 0; current < index && iterator.rem != 0; ++current) {
            xcb_screen_next(&iterator);
        }
        return iterator.rem == 0 ? nullptr : iterator.data;
    }

    auto XcbConnection::check(xcb_void_cookie_t cookie) const -> ilias::IoResult<void>
    {
        auto *error = xcb_request_check(mConnection, cookie);
        if (!error) {
            return {};
        }
        std::free(error);
        return ilias::Err(std::make_error_code(std::errc::io_error));
    }

    auto XcbConnection::flush() const -> ilias::IoResult<void>
    {
        if (xcb_flush(mConnection) > 0) {
            return {};
        }
        const auto error = xcb_connection_has_error(mConnection);
        return ilias::Err(error == 0 ? std::make_error_code(std::errc::io_error)
                                     : connectionError(error));
    }

} // namespace mks
