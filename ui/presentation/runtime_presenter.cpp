#include "presentation/runtime_presenter.hpp"
#include "preinclude.hpp"

#include "app/client.hpp"
#include "app/server.hpp"
#include "platform/platform.hpp"

#include <QCoreApplication>
#include <QFileInfo>
#include <QMetaObject>
#include <QPointer>
#include <QSettings>
#include <spdlog/sinks/base_sink.h>

#include <functional>

namespace mks::gui
{

    namespace
    {
        constexpr auto kMaximumLogLength = 64 * 1024;

        struct EndpointSetting {
            QString host;
            int     port = 24800;
        };

        auto parseEndpointSetting(const QString &text, const QString &fallbackHost,
                                  int fallbackPort) -> EndpointSetting
        {
            const auto trimmed  = text.trimmed();
            auto       host     = QString{};
            auto       portText = QString{};

            if (trimmed.startsWith(QLatin1Char('['))) {
                const auto bracket = trimmed.indexOf(QLatin1Char(']'));
                if (bracket > 1 && bracket + 1 < trimmed.size() &&
                    trimmed[bracket + 1] == QLatin1Char(':')) {
                    host     = trimmed.mid(1, bracket - 1);
                    portText = trimmed.mid(bracket + 2);
                }
            }
            else {
                const auto colon = trimmed.lastIndexOf(QLatin1Char(':'));
                if (colon > 0) {
                    host     = trimmed.left(colon);
                    portText = trimmed.mid(colon + 1);
                }
            }

            auto       validPort = false;
            const auto port      = portText.toInt(&validPort);
            if (host.isEmpty() || !validPort || port < 1 || port > 65535) {
                return {.host = fallbackHost, .port = fallbackPort};
            }
            return {.host = std::move(host), .port = port};
        }

        auto loadEndpointSetting(QSettings &settings, const QString &prefix,
                                 const QString &fallbackHost, int fallbackPort) -> EndpointSetting
        {
            const auto hostKey = prefix + QStringLiteral("Host");
            const auto portKey = prefix + QStringLiteral("Port");
            if (settings.contains(hostKey) && settings.contains(portKey)) {
                const auto host = settings.value(hostKey, fallbackHost).toString();
                const auto port = settings.value(portKey, fallbackPort).toInt();
                if (!host.trimmed().isEmpty() && port >= 1 && port <= 65535) {
                    return {.host = host, .port = port};
                }
            }

            // Migrate the combined HOST:PORT setting used by the first GUI revision.
            return parseEndpointSetting(
                settings.value(prefix + QStringLiteral("Endpoint")).toString(), fallbackHost,
                fallbackPort);
        }

        auto parseLogLevel(const QString &text) -> spdlog::level::level_enum
        {
            const auto normalized = text.trimmed().toLower();
            if (normalized == QStringLiteral("trace")) {
                return spdlog::level::trace;
            }
            if (normalized == QStringLiteral("debug")) {
                return spdlog::level::debug;
            }
            if (normalized == QStringLiteral("warn")) {
                return spdlog::level::warn;
            }
            if (normalized == QStringLiteral("error")) {
                return spdlog::level::err;
            }
            if (normalized == QStringLiteral("critical")) {
                return spdlog::level::critical;
            }
            if (normalized == QStringLiteral("off")) {
                return spdlog::level::off;
            }
            return spdlog::level::info;
        }

        class QtLogSink final : public spdlog::sinks::base_sink<std::mutex> {
        public:
            using Callback = std::function<void(QString)>;

            explicit QtLogSink(Callback callback) : mCallback(std::move(callback)) {}

        protected:
            auto sink_it_(const spdlog::details::log_msg &message) -> void override
            {
                auto formatted = spdlog::memory_buf_t{};
                formatter_->format(message, formatted);
                mCallback(
                    QString::fromUtf8(formatted.data(), static_cast<qsizetype>(formatted.size())));
            }

            auto flush_() -> void override {}

        private:
            Callback mCallback;
        };
    } // namespace

    RuntimePresenter::RuntimePresenter(QObject *parent) : QObject(parent)
    {
        auto settings = QSettings{};
        mSelectedMode = static_cast<RunMode>(
            settings.value(QStringLiteral("runtime/mode"), ServerMode).toInt());
        const auto server = loadEndpointSetting(settings, QStringLiteral("runtime/server"),
                                                QStringLiteral("0.0.0.0"), 24800);
        const auto client = loadEndpointSetting(settings, QStringLiteral("runtime/client"),
                                                QStringLiteral("127.0.0.1"), 24800);
        mServerHost       = server.host;
        mServerPort       = server.port;
        mClientHost       = client.host;
        mClientPort       = client.port;
        mLogLevel =
            settings.value(QStringLiteral("runtime/logLevel"), QStringLiteral("info")).toString();
        mStatusTitle  = tr("未运行");
        mStatusDetail = tr("选择运行模式并启动 mksync。");

        mActiveScreenTimer.setInterval(150);
        connect(&mActiveScreenTimer, &QTimer::timeout, this,
                &RuntimePresenter::refreshActiveScreen);

        const auto guardedThis = QPointer<RuntimePresenter>{this};
        mLogSink               = std::make_shared<QtLogSink>([guardedThis](QString text) {
            if (!guardedThis) {
                return;
            }
            QMetaObject::invokeMethod(
                guardedThis,
                [guardedThis, text = std::move(text)] {
                    if (guardedThis) {
                        guardedThis->appendLog(text);
                    }
                },
                Qt::QueuedConnection);
        });
        mLogSink->set_pattern("[%H:%M:%S] [%l] %v");
        if (auto logger = spdlog::default_logger()) {
            logger->sinks().push_back(mLogSink);
        }
    }

    RuntimePresenter::~RuntimePresenter()
    {
        if (auto logger = spdlog::default_logger()) {
            std::erase(logger->sinks(), mLogSink);
        }
        // Window closing is delayed until runService() observes the stop token.
        Q_ASSERT(!active());
    }

    auto RuntimePresenter::selectedMode() const -> int
    {
        return mSelectedMode;
    }

    auto RuntimePresenter::serverHost() const -> QString
    {
        return mServerHost;
    }

    auto RuntimePresenter::serverPort() const -> int
    {
        return mServerPort;
    }

    auto RuntimePresenter::clientHost() const -> QString
    {
        return mClientHost;
    }

    auto RuntimePresenter::clientPort() const -> int
    {
        return mClientPort;
    }

    auto RuntimePresenter::logLevel() const -> QString
    {
        return mLogLevel;
    }

    auto RuntimePresenter::active() const -> bool
    {
        return mState != RunState::Idle;
    }

    auto RuntimePresenter::running() const -> bool
    {
        return mState == RunState::Running;
    }

    auto RuntimePresenter::statusTitle() const -> QString
    {
        return mStatusTitle;
    }

    auto RuntimePresenter::statusDetail() const -> QString
    {
        return mStatusDetail;
    }

    auto RuntimePresenter::logText() const -> QString
    {
        return mLogText;
    }

    auto RuntimePresenter::hasActiveScreen() const -> bool
    {
        return !mActiveScreenOwnerId.isEmpty() && mActiveScreenIndex >= 0;
    }

    auto RuntimePresenter::activeScreenOwnerId() const -> QString
    {
        return mActiveScreenOwnerId;
    }

    auto RuntimePresenter::activeScreenIndex() const -> int
    {
        return mActiveScreenIndex;
    }

    auto RuntimePresenter::setSelectedMode(int mode) -> void
    {
        const auto nextMode = mode == ClientMode ? ClientMode : ServerMode;
        if (mSelectedMode == nextMode || active()) {
            return;
        }
        mSelectedMode = nextMode;
        QSettings{}.setValue(QStringLiteral("runtime/mode"), mSelectedMode);
        emit selectedModeChanged();
    }

    auto RuntimePresenter::setServerHost(const QString &host) -> void
    {
        const auto trimmed = host.trimmed();
        if (trimmed.isEmpty() || mServerHost == trimmed || active()) {
            return;
        }
        mServerHost = trimmed;
        QSettings{}.setValue(QStringLiteral("runtime/serverHost"), trimmed);
        emit serverHostChanged();
    }

    auto RuntimePresenter::setServerPort(int port) -> void
    {
        if (port < 1 || port > 65535 || mServerPort == port || active()) {
            return;
        }
        mServerPort = port;
        QSettings{}.setValue(QStringLiteral("runtime/serverPort"), port);
        emit serverPortChanged();
    }

    auto RuntimePresenter::setClientHost(const QString &host) -> void
    {
        const auto trimmed = host.trimmed();
        if (trimmed.isEmpty() || mClientHost == trimmed || active()) {
            return;
        }
        mClientHost = trimmed;
        QSettings{}.setValue(QStringLiteral("runtime/clientHost"), trimmed);
        emit clientHostChanged();
    }

    auto RuntimePresenter::setClientPort(int port) -> void
    {
        if (port < 1 || port > 65535 || mClientPort == port || active()) {
            return;
        }
        mClientPort = port;
        QSettings{}.setValue(QStringLiteral("runtime/clientPort"), port);
        emit clientPortChanged();
    }

    auto RuntimePresenter::setLogLevel(const QString &level) -> void
    {
        static const auto levels     = QStringList{QStringLiteral("trace"), QStringLiteral("debug"),
                                               QStringLiteral("info"),  QStringLiteral("warn"),
                                               QStringLiteral("error"), QStringLiteral("critical"),
                                               QStringLiteral("off")};
        const auto        normalized = level.trimmed().toLower();
        if (!levels.contains(normalized) || mLogLevel == normalized || active()) {
            return;
        }
        mLogLevel = normalized;
        QSettings{}.setValue(QStringLiteral("runtime/logLevel"), normalized);
        emit logLevelChanged();
    }

    auto RuntimePresenter::start(const QString &configPath) -> void
    {
        if (active()) {
            return;
        }

        const auto host          = mSelectedMode == ServerMode ? mServerHost : mClientHost;
        const auto port          = mSelectedMode == ServerMode ? mServerPort : mClientPort;
        const auto endpointValue = endpointText(host, port);
        const auto endpoint      = ilias::IPEndpoint::fromString(endpointValue.toStdString());
        if (!endpoint) {
            setStatus(tr("无法启动"), tr("请输入有效的 IP 地址和端口。"));
            return;
        }
        if (configPath.trimmed().isEmpty()) {
            setStatus(tr("无法启动"), tr("当前配置尚未保存，请先保存配置文件。"));
            return;
        }

        const auto absoluteConfigPath =
            std::filesystem::path{QFileInfo{configPath}.absoluteFilePath().toStdString()};
        auto config = loadConfig(absoluteConfigPath);
        if (!config) {
            setStatus(tr("无法读取配置"), QString::fromStdString(config.error().message()));
            return;
        }

        mStopping          = false;
        mCompletedNormally = false;
        mStopSource        = std::stop_source{};
        mRunError.clear();
        mLogText.clear();
        emit logTextChanged();

        mState = RunState::Starting;
        setStatus(tr("正在启动"), mSelectedMode == ServerMode
                                      ? tr("正在准备监听 %1").arg(endpointValue)
                                      : tr("正在连接 %1").arg(endpointValue));
        appendLog(
            mSelectedMode == ServerMode
                ? tr("[GUI] 以服务端模式启动，监听 %1，日志等级 %2\n").arg(endpointValue, mLogLevel)
                : tr("[GUI] 以客户端模式启动，连接 %1，日志等级 %2\n")
                      .arg(endpointValue, mLogLevel));

        mRunHandle = ilias::spawn(runService(*endpoint, std::move(*config), absoluteConfigPath));
    }

    auto RuntimePresenter::stop() -> void
    {
        if (!active() || mStopping) {
            return;
        }
        mStopping = true;
        mState    = RunState::Stopping;
        setStatus(tr("正在停止"), tr("正在安全关闭键鼠同步服务…"));
        appendLog(tr("[GUI] 已请求停止。\n"));
        mStopSource.request_stop();
    }

    auto RuntimePresenter::clearLog() -> void
    {
        if (mLogText.isEmpty()) {
            return;
        }
        mLogText.clear();
        emit logTextChanged();
    }

    auto RuntimePresenter::quitApplication() -> void
    {
        if (!active()) {
            QCoreApplication::quit();
            return;
        }
        mQuitWhenStopped = true;
        stop();
    }

    auto RuntimePresenter::runService(ilias::IPEndpoint endpoint, AppConfig config,
                                      std::filesystem::path configPath) -> Task<void>
    {
        co_await runServiceBody(endpoint, std::move(config), std::move(configPath));
        finishRun();
    }

    auto RuntimePresenter::runServiceBody(ilias::IPEndpoint endpoint, AppConfig config,
                                          std::filesystem::path configPath) -> Task<void>
    {
        try {
            spdlog::set_level(parseLogLevel(mLogLevel));
            auto platform = Platform::create();
            if (!platform) {
                mRunError = tr("当前系统无法创建键鼠输入后端。");
                co_return;
            }

            mState = RunState::Running;
            setStatus(mSelectedMode == ServerMode ? tr("服务端运行中") : tr("客户端运行中"),
                      mSelectedMode == ServerMode
                          ? tr("正在监听 %1").arg(endpointText(mServerHost, mServerPort))
                          : tr("已连接到 %1").arg(endpointText(mClientHost, mClientPort)));
            appendLog(tr("[GUI] 输入后端已初始化。\n"));

            if (mSelectedMode == ServerMode) {
                auto server =
                    Server{std::move(platform), endpoint, std::move(config), std::move(configPath)};
                mRunningServer = &server;
                mActiveScreenTimer.start();
                refreshActiveScreen();
                auto [canceled, result] =
                    co_await ilias::whenAny(mStopSource.get_token(), server.run());
                if (result) {
                    mCompletedNormally = true;
                    if (!*result) {
                        mRunError = QString::fromStdString(result->error().message());
                    }
                }
                else if (canceled) {
                    mStopping = true;
                }
            }
            else {
                auto client = Client{std::move(platform), endpoint, std::move(config)};
                auto [canceled, result] =
                    co_await ilias::whenAny(mStopSource.get_token(), client.run());
                if (result) {
                    mCompletedNormally = true;
                    if (!*result) {
                        mRunError = QString::fromStdString(result->error().message());
                    }
                }
                else if (canceled) {
                    mStopping = true;
                }
            }
        }
        catch (const std::exception &error) {
            mRunError = QString::fromLocal8Bit(error.what());
        }
        clearRunningServer();
    }

    auto RuntimePresenter::finishRun() -> void
    {
        if (mStopping) {
            appendLog(tr("[GUI] mksync 已停止。\n"));
            setStatus(tr("已停止"), tr("键鼠同步服务已安全关闭。"));
        }
        else if (!mRunError.isEmpty()) {
            appendLog(tr("[GUI] 运行失败：%1\n").arg(mRunError));
            setStatus(tr("运行失败"), mRunError);
        }
        else if (mCompletedNormally) {
            appendLog(tr("[GUI] 服务已退出。\n"));
            setStatus(tr("服务已退出"), tr("mksync 已结束运行。"));
        }
        else {
            setStatus(tr("已停止"), tr("键鼠同步服务已结束。"));
        }

        mStopping = false;
        mState    = RunState::Idle;
        emit stateChanged();

        if (mQuitWhenStopped) {
            QCoreApplication::quit();
        }
    }

    auto RuntimePresenter::appendLog(const QString &text) -> void
    {
        mLogText.append(text);
        if (mLogText.size() > kMaximumLogLength) {
            mLogText.remove(0, mLogText.size() - kMaximumLogLength);
        }
        emit logTextChanged();
    }

    auto RuntimePresenter::setStatus(const QString &title, const QString &detail) -> void
    {
        mStatusTitle  = title;
        mStatusDetail = detail;
        emit stateChanged();
    }

    auto RuntimePresenter::refreshActiveScreen() -> void
    {
        auto ownerId     = QString{};
        auto screenIndex = -1;
        if (mRunningServer != nullptr) {
            if (const auto key = mRunningServer->activeScreenKey()) {
                ownerId     = QString::fromStdString(key->ownerId);
                screenIndex = static_cast<int>(key->screenIndex);
            }
        }

        if (mActiveScreenOwnerId == ownerId && mActiveScreenIndex == screenIndex) {
            return;
        }
        mActiveScreenOwnerId = std::move(ownerId);
        mActiveScreenIndex   = screenIndex;
        emit activeScreenChanged();
    }

    auto RuntimePresenter::clearRunningServer() -> void
    {
        mActiveScreenTimer.stop();
        mRunningServer = nullptr;
        refreshActiveScreen();
    }

    auto RuntimePresenter::endpointText(const QString &host, int port) -> QString
    {
        auto normalizedHost = host.trimmed();
        if (normalizedHost.contains(QLatin1Char(':')) &&
            !(normalizedHost.startsWith(QLatin1Char('[')) &&
              normalizedHost.endsWith(QLatin1Char(']')))) {
            normalizedHost = QStringLiteral("[%1]").arg(normalizedHost);
        }
        return QStringLiteral("%1:%2").arg(normalizedHost).arg(port);
    }

} // namespace mks::gui
