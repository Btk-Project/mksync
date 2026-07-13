#pragma once

#include "config/app_config.hpp"
#include "config/arg_config.hpp"
#include "preinclude.hpp"

#include <ilias/task/spawn.hpp>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <spdlog/sinks/sink.h>

#include <stop_token>

namespace mks
{
    class Server;
}

namespace mks::gui
{

    class RuntimePresenter final : public QObject {
        Q_OBJECT

        Q_PROPERTY(
            int selectedMode READ selectedMode WRITE setSelectedMode NOTIFY selectedModeChanged)
        Q_PROPERTY(QString endpoint READ endpoint WRITE setEndpoint NOTIFY endpointChanged)
        Q_PROPERTY(QString logLevel READ logLevel WRITE setLogLevel NOTIFY logLevelChanged)
        Q_PROPERTY(QString selectedBackend READ selectedBackend WRITE setSelectedBackend NOTIFY
                       selectedBackendChanged)
        Q_PROPERTY(QStringList backendOptions READ backendOptions CONSTANT)
        Q_PROPERTY(QString appConfigPath READ appConfigPath WRITE setAppConfigPath NOTIFY
                       appConfigPathChanged)
        Q_PROPERTY(QString startupConfigPath READ startupConfigPath NOTIFY startupConfigPathChanged)
        Q_PROPERTY(QString startupStatusMessage READ startupStatusMessage NOTIFY
                       startupStatusMessageChanged)
        Q_PROPERTY(bool hasStartupError READ hasStartupError NOTIFY startupStatusMessageChanged)
        Q_PROPERTY(bool active READ active NOTIFY stateChanged)
        Q_PROPERTY(bool running READ running NOTIFY stateChanged)
        Q_PROPERTY(QString statusTitle READ statusTitle NOTIFY stateChanged)
        Q_PROPERTY(QString statusDetail READ statusDetail NOTIFY stateChanged)
        Q_PROPERTY(QString backendName READ backendName NOTIFY backendStatusChanged)
        Q_PROPERTY(QString backendCheckText READ backendCheckText NOTIFY backendStatusChanged)
        Q_PROPERTY(bool backendChecking READ backendChecking NOTIFY backendStatusChanged)
        Q_PROPERTY(QString logText READ logText NOTIFY logTextChanged)
        Q_PROPERTY(bool hasActiveScreen READ hasActiveScreen NOTIFY activeScreenChanged)
        Q_PROPERTY(QString activeScreenOwnerId READ activeScreenOwnerId NOTIFY activeScreenChanged)
        Q_PROPERTY(int activeScreenIndex READ activeScreenIndex NOTIFY activeScreenChanged)

    public:
        enum RunMode
        {
            ServerMode = 0,
            ClientMode = 1,
        };
        Q_ENUM(RunMode)

        explicit RuntimePresenter(QObject *parent = nullptr);
        ~RuntimePresenter() override;

        auto selectedMode() const -> int;
        auto endpoint() const -> QString;
        auto logLevel() const -> QString;
        auto selectedBackend() const -> QString;
        auto backendOptions() const -> QStringList;
        auto appConfigPath() const -> QString;
        auto startupConfigPath() const -> QString;
        auto startupStatusMessage() const -> QString;
        auto hasStartupError() const -> bool;
        auto active() const -> bool;
        auto running() const -> bool;
        auto statusTitle() const -> QString;
        auto statusDetail() const -> QString;
        auto backendName() const -> QString;
        auto backendCheckText() const -> QString;
        auto backendChecking() const -> bool;
        auto logText() const -> QString;
        auto hasActiveScreen() const -> bool;
        auto activeScreenOwnerId() const -> QString;
        auto activeScreenIndex() const -> int;

        auto setSelectedMode(int mode) -> void;
        auto setEndpoint(const QString &endpoint) -> void;
        auto setLogLevel(const QString &level) -> void;
        auto setSelectedBackend(const QString &backend) -> void;
        auto setAppConfigPath(const QString &path) -> void;

        Q_INVOKABLE void createStartupConfig();
        Q_INVOKABLE void importStartupConfig(const QUrl &url);
        Q_INVOKABLE void saveStartupConfig();
        Q_INVOKABLE void saveStartupConfigAs(const QUrl &url);
        Q_INVOKABLE void start();
        Q_INVOKABLE void stop();
        Q_INVOKABLE void clearLog();
        Q_INVOKABLE void refreshBackendChecks();
        Q_INVOKABLE void quitApplication();

    signals:
        void selectedModeChanged();
        void endpointChanged();
        void logLevelChanged();
        void selectedBackendChanged();
        void appConfigPathChanged();
        void startupConfigPathChanged();
        void startupStatusMessageChanged();
        void stateChanged();
        void logTextChanged();
        void activeScreenChanged();
        void backendStatusChanged();

    private:
        enum class RunState
        {
            Idle,
            Starting,
            Running,
            Stopping,
        };

        auto        runService(ilias::IPEndpoint endpoint, AppConfig config,
                               std::filesystem::path configPath) -> Task<void>;
        auto        runServiceBody(ilias::IPEndpoint endpoint, AppConfig config,
                                   std::filesystem::path configPath) -> Task<void>;
        auto        runBackendChecks() -> Task<void>;
        auto        finishRun() -> void;
        auto        appendLog(const QString &text) -> void;
        auto        setStatus(const QString &title, const QString &detail) -> void;
        auto        publishCommand() -> void;
        auto        showStartupSuccess(const QString &message) -> void;
        auto        showStartupError(const QString &action, const std::error_code &error) -> void;
        auto        showStartupValidationError(const QString &message) -> void;
        auto        refreshActiveScreen() -> void;
        auto        clearRunningServer() -> void;
        auto        commonConfig() const -> const CommonConfig &;
        auto        commonConfig() -> CommonConfig &;
        auto        endpointValue() const -> const std::string &;
        auto        endpointValue() -> std::string &;
        static auto localPath(const QUrl &url) -> std::optional<std::filesystem::path>;

        ilias::StopHandle     mRunHandle;
        ilias::StopHandle     mBackendCheckHandle;
        std::stop_source      mStopSource;
        CliCommand            mCommand = ServerCommand{.endpoint = "0.0.0.0:1234", .common = {}};
        std::filesystem::path mStartupConfigPath;
        RunState              mState = RunState::Idle;
        QString               mStartupStatusMessage;
        bool                  mHasStartupError = false;
        QString               mStatusTitle;
        QString               mStatusDetail;
        QString               mLogText;
        QString               mRunError;
        QString               mActiveBackendName;
        QString               mBackendCheckText;
        bool                  mBackendChecking = false;
        QTimer                mActiveScreenTimer;
        mks::Server          *mRunningServer = nullptr;
        QString               mActiveScreenOwnerId;
        int                   mActiveScreenIndex = -1;
        bool                  mStopping          = false;
        bool                  mCompletedNormally = false;
        bool                  mQuitWhenStopped   = false;
        std::shared_ptr<spdlog::sinks::sink> mLogSink;
    };

} // namespace mks::gui
