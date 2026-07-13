#include "presentation/runtime_presenter.hpp"
#include "preinclude.hpp"

#include "app/client.hpp"
#include "app/server.hpp"
#include "platform/platform.hpp"

#include <QCoreApplication>
#include <QFileInfo>
#include <QMetaObject>
#include <QPointer>
#include <spdlog/sinks/base_sink.h>

#include <functional>

namespace mks::gui
{

    namespace
    {
        constexpr auto kMaximumLogLength = 64 * 1024;

        auto parseLogLevel(const QString &text) -> spdlog::level::level_enum
        {
            const auto normalized = text.trimmed().toLower();
            if (normalized == QStringLiteral("trace")) {
                return spdlog::level::trace;
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
        mStatusTitle          = tr("未运行");
        mStatusDetail         = tr("选择运行模式并启动 mksync。");
        mStartupStatusMessage = tr("启动参数已就绪，可导出为命令行兼容的 TOML 文件。");

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
        return std::holds_alternative<ClientCommand>(mCommand) ? ClientMode : ServerMode;
    }

    auto RuntimePresenter::endpoint() const -> QString
    {
        return QString::fromStdString(endpointValue());
    }

    auto RuntimePresenter::logLevel() const -> QString
    {
        return QString::fromStdString(commonConfig().logLevel);
    }

    auto RuntimePresenter::appConfigPath() const -> QString
    {
        return QString::fromStdString(commonConfig().configPath);
    }

    auto RuntimePresenter::startupConfigPath() const -> QString
    {
        return QString::fromStdString(mStartupConfigPath.string());
    }

    auto RuntimePresenter::startupStatusMessage() const -> QString
    {
        return mStartupStatusMessage;
    }

    auto RuntimePresenter::hasStartupError() const -> bool
    {
        return mHasStartupError;
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
        if (selectedMode() == nextMode || active()) {
            return;
        }
        auto endpointText = endpointValue();
        auto common       = commonConfig();
        if (nextMode == ClientMode) {
            mCommand =
                ClientCommand{.endpoint = std::move(endpointText), .common = std::move(common)};
        }
        else {
            mCommand =
                ServerCommand{.endpoint = std::move(endpointText), .common = std::move(common)};
        }
        publishCommand();
    }

    auto RuntimePresenter::setEndpoint(const QString &endpointText) -> void
    {
        const auto trimmed = endpointText.trimmed();
        if (trimmed.isEmpty() || endpoint() == trimmed || active()) {
            return;
        }
        endpointValue() = trimmed.toStdString();
        emit endpointChanged();
    }

    auto RuntimePresenter::setLogLevel(const QString &level) -> void
    {
        static const auto levels =
            QStringList{QStringLiteral("trace"), QStringLiteral("info"),     QStringLiteral("warn"),
                        QStringLiteral("error"), QStringLiteral("critical"), QStringLiteral("off")};
        const auto normalized = level.trimmed().toLower();
        if (!levels.contains(normalized) || logLevel() == normalized || active()) {
            return;
        }
        commonConfig().logLevel = normalized.toStdString();
        emit logLevelChanged();
    }

    auto RuntimePresenter::setAppConfigPath(const QString &path) -> void
    {
        const auto normalized = path.trimmed();
        if (appConfigPath() == normalized) {
            return;
        }
        commonConfig().configPath = normalized.toStdString();
        emit appConfigPathChanged();
    }

    auto RuntimePresenter::createStartupConfig() -> void
    {
        if (active()) {
            return;
        }
        mCommand = ServerCommand{.endpoint = "0.0.0.0:1234", .common = {}};
        mStartupConfigPath.clear();
        publishCommand();
        emit startupConfigPathChanged();
        showStartupSuccess(tr("已创建新的服务端启动参数。选择“导出…”以写入 TOML 文件。"));
    }

    auto RuntimePresenter::importStartupConfig(const QUrl &url) -> void
    {
        if (active()) {
            showStartupValidationError(tr("请先停止正在运行的服务，再导入启动参数。"));
            return;
        }
        const auto path = localPath(url);
        if (!path) {
            showStartupValidationError(tr("只能导入本地 TOML 文件。"));
            return;
        }
        auto imported = importCliCommand(*path);
        if (!imported) {
            showStartupError(tr("导入启动参数失败"), imported.error());
            return;
        }
        if (std::holds_alternative<CheckPlatformCommand>(*imported)) {
            showStartupValidationError(tr("GUI 只能载入 server 或 client 启动参数。"));
            return;
        }
        mCommand           = std::move(*imported);
        mStartupConfigPath = *path;
        publishCommand();
        emit startupConfigPathChanged();
        showStartupSuccess(tr("已导入 %1").arg(startupConfigPath()));
    }

    auto RuntimePresenter::saveStartupConfig() -> void
    {
        if (mStartupConfigPath.empty()) {
            showStartupValidationError(tr("启动参数尚未指定文件。请选择“导出…”。"));
            return;
        }
        auto saved = exportCliCommand(mStartupConfigPath, mCommand);
        if (!saved) {
            showStartupError(tr("导出启动参数失败"), saved.error());
            return;
        }
        showStartupSuccess(tr("已导出到 %1").arg(startupConfigPath()));
    }

    auto RuntimePresenter::saveStartupConfigAs(const QUrl &url) -> void
    {
        const auto path = localPath(url);
        if (!path) {
            showStartupValidationError(tr("请选择本地 TOML 保存位置。"));
            return;
        }
        auto saved = exportCliCommand(*path, mCommand);
        if (!saved) {
            showStartupError(tr("导出启动参数失败"), saved.error());
            return;
        }
        mStartupConfigPath = *path;
        emit startupConfigPathChanged();
        showStartupSuccess(tr("已导出到 %1").arg(startupConfigPath()));
    }

    auto RuntimePresenter::start() -> void
    {
        if (active()) {
            return;
        }

        const auto endpointText = endpoint();
        const auto endpoint     = ilias::IPEndpoint::fromString(endpointValue());
        if (!endpoint) {
            setStatus(tr("无法启动"), tr("请输入有效的 IP 地址和端口。"));
            return;
        }
        const auto configPath = appConfigPath();
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
        setStatus(tr("正在启动"), selectedMode() == ServerMode
                                      ? tr("正在准备监听 %1").arg(endpointText)
                                      : tr("正在连接 %1").arg(endpointText));
        appendLog(
            selectedMode() == ServerMode
                ? tr("[GUI] 以服务端模式启动，监听 %1，日志等级 %2\n").arg(endpointText, logLevel())
                : tr("[GUI] 以客户端模式启动，连接 %1，日志等级 %2\n")
                      .arg(endpointText, logLevel()));

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
            spdlog::set_level(parseLogLevel(logLevel()));
            auto platform = Platform::create();
            if (!platform) {
                mRunError = tr("当前系统无法创建键鼠输入后端。");
                co_return;
            }

            mState = RunState::Running;
            setStatus(selectedMode() == ServerMode ? tr("服务端运行中") : tr("客户端运行中"),
                      selectedMode() == ServerMode ? tr("正在监听 %1").arg(this->endpoint())
                                                   : tr("已连接到 %1").arg(this->endpoint()));
            appendLog(tr("[GUI] 输入后端已初始化。\n"));

            if (selectedMode() == ServerMode) {
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

    auto RuntimePresenter::publishCommand() -> void
    {
        emit selectedModeChanged();
        emit endpointChanged();
        emit logLevelChanged();
        emit appConfigPathChanged();
    }

    auto RuntimePresenter::showStartupSuccess(const QString &message) -> void
    {
        mStartupStatusMessage = message;
        mHasStartupError      = false;
        emit startupStatusMessageChanged();
    }

    auto RuntimePresenter::showStartupError(const QString &action, const std::error_code &error)
        -> void
    {
        mStartupStatusMessage = tr("%1：%2").arg(action, QString::fromStdString(error.message()));
        mHasStartupError      = true;
        emit startupStatusMessageChanged();
    }

    auto RuntimePresenter::showStartupValidationError(const QString &message) -> void
    {
        mStartupStatusMessage = message;
        mHasStartupError      = true;
        emit startupStatusMessageChanged();
    }

    auto RuntimePresenter::commonConfig() const -> const CommonConfig &
    {
        if (const auto *server = std::get_if<ServerCommand>(&mCommand)) {
            return server->common;
        }
        return std::get<ClientCommand>(mCommand).common;
    }

    auto RuntimePresenter::commonConfig() -> CommonConfig &
    {
        if (auto *server = std::get_if<ServerCommand>(&mCommand)) {
            return server->common;
        }
        return std::get<ClientCommand>(mCommand).common;
    }

    auto RuntimePresenter::endpointValue() const -> const std::string &
    {
        if (const auto *server = std::get_if<ServerCommand>(&mCommand)) {
            return server->endpoint;
        }
        return std::get<ClientCommand>(mCommand).endpoint;
    }

    auto RuntimePresenter::endpointValue() -> std::string &
    {
        if (auto *server = std::get_if<ServerCommand>(&mCommand)) {
            return server->endpoint;
        }
        return std::get<ClientCommand>(mCommand).endpoint;
    }

    auto RuntimePresenter::localPath(const QUrl &url) -> std::optional<std::filesystem::path>
    {
        if (!url.isLocalFile()) {
            return std::nullopt;
        }
        return std::filesystem::path{url.toLocalFile().toStdString()};
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

} // namespace mks::gui
