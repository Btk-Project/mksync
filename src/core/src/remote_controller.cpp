#include "mksync/core/remote_controller.hpp"

#include <ilias/net/tcp.hpp>
#include <ilias/net/udp.hpp>
#include <ilias/fs/pipe.hpp>
#include <ilias/sync/scope.hpp>

#include "mksync/base/app.hpp"
#include "mksync/base/default_configs.hpp"

namespace mks::base
{
    using ::ilias::Error;
    using ::ilias::Event;
    using ::ilias::IoTask;
    using ::ilias::IPEndpoint;
    using ::ilias::Pipe;
    using ::ilias::Result;
    using ::ilias::Task;
    using ::ilias::TaskScope;
    using ::ilias::TcpClient;
    using ::ilias::TcpListener;
    using ::ilias::UdpClient;
    using ::ilias::Unexpected;

    class RemoteControllerImp {
    public:
        RemoteControllerImp(IApp *app) : _app(app), _taskScop(*app->get_io_context()) {}
        ~RemoteControllerImp() {}
        virtual auto setup() -> Task<int>    = 0;
        virtual auto teardown() -> Task<int> = 0;
        virtual auto loop() -> Task<void>
        {
            while (true) {
                if (auto ret = co_await read(); ret) {
                    co_await send(co_await _app->command_invoker().execute(ret.value()));
                }
                else if (ret.error() == Error::Canceled) {
                    co_await send("error: " + std::string(ret.error().message()));
                }
                else {
                    co_return;
                }
            }
        }
        virtual auto read() -> IoTask<std::string>          = 0;
        virtual auto send(std::string_view) -> IoTask<void> = 0;

        static auto create(IApp *app, std::string_view param)
            -> std::unique_ptr<RemoteControllerImp>;

    protected:
        IApp     *_app;
        TaskScope _taskScop;
    };

    class TcpRemoteController : public RemoteControllerImp {
    public:
        TcpRemoteController(IApp *app, std::string_view address)
            : RemoteControllerImp(app), _ipendpoint(address), _taskScop(*app->get_io_context())
        {
            _taskScop.setAutoCancel(true);
        }
        ~TcpRemoteController() {}
        auto setup() -> Task<int> override
        {
            if (!_ipendpoint.isValid()) {
                co_return -1;
            }
            _listener = (co_await TcpListener::make(_ipendpoint.family())).value_or(TcpListener());
            if (_listener) {
                _taskScop.spawn(accept());
                co_return 0;
            }
            co_return -1;
        }

        auto teardown() -> Task<int> override
        {
            if (_listener) {
                _listener.close();
            }
            if (_client) {
                _client.close();
            }
            _taskScop.cancel();
            co_await _taskScop;
            co_return 0;
        }

        auto read() -> IoTask<std::string> override
        {
            while (!_client) {
                _sync.clear();
                if (auto ret = co_await _sync; !ret) {
                    co_return Unexpected<Error>(ret.error());
                }
            }
            std::array<std::byte, 2048> buffer;
            if (_client) {
                memset(buffer.data(), 0, buffer.size());
                if (auto ret = co_await _client.read(buffer); ret) {
                    co_return std::string(reinterpret_cast<char *>(buffer.data()), ret.value());
                }
                else {
                    co_return Unexpected<Error>(ret.error());
                }
            }
            co_return "";
        }

        auto send(std::string_view data) -> IoTask<void> override
        {
            while (!_client) {
                _sync.clear();
                if (auto ret = co_await _sync; !ret) {
                    co_return ret;
                }
            }
            if (_client) {
                auto ret = co_await _client.write(
                    {reinterpret_cast<const std::byte *>(data.data()), data.length()});
                if (ret && ret.value() != data.length()) {
                    co_return co_await send(data.substr(ret.value()));
                }
                else if (!ret) {
                    SPDLOG_ERROR("send data failed, error : {}", ret.error().message());
                }
            }
            co_return {};
        }

        auto accept() -> Task<void>
        {
            while (true) {
                auto client = co_await _listener.accept();
                if (_client) {
                    _client.close();
                }
                else {
                    co_return;
                }
                if (client) {
                    _client = std::move(client.value().first);
                    _sync.set();
                }
            }
        }

    private:
        IPEndpoint  _ipendpoint;
        TcpListener _listener;
        TcpClient   _client;
        TaskScope   _taskScop;
        Event       _sync;
    };

    auto RemoteControllerImp::create(IApp *app, std::string_view param)
        -> std::unique_ptr<RemoteControllerImp>
    {
        if (param.starts_with("tcp:")) {
            return std::make_unique<TcpRemoteController>(app, param.substr(4));
        }
        return 0;
    }

    RemoteController::RemoteController(IApp *app) : _app(app), _taskScop(*app->get_io_context())
    {
        _taskScop.setAutoCancel(true);
    }

    RemoteController::~RemoteController() {}

    auto RemoteController::setup() -> Task<int>
    {
        if (_imp == nullptr) {
            _imp = RemoteControllerImp::create(
                _app, _app->settings().get(remote_controller_config_name,
                                           remote_controller_default_value));
        }
        if (_imp == nullptr) {
            co_return -1;
        }
        co_return co_await _imp->setup();
    }

    auto RemoteController::teardown() -> Task<int>
    {
        if (_imp) {
            auto ret = co_await _imp->teardown();
            _imp.reset();
            co_return ret;
        }
        co_return -1;
    }

    auto RemoteController::name() -> const char *
    {
        return "RemoteController";
    }
} // namespace mks::base