#pragma once

#include "config/app_config.hpp"
#include "preinclude.hpp"

#include <ilias/task/spawn.hpp>

#include <QObject>
#include <QString>
#include <QTimer>
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
        Q_PROPERTY(QString serverHost READ serverHost WRITE setServerHost NOTIFY serverHostChanged)
        Q_PROPERTY(int serverPort READ serverPort WRITE setServerPort NOTIFY serverPortChanged)
        Q_PROPERTY(QString clientHost READ clientHost WRITE setClientHost NOTIFY clientHostChanged)
        Q_PROPERTY(int clientPort READ clientPort WRITE setClientPort NOTIFY clientPortChanged)
        Q_PROPERTY(QString logLevel READ logLevel WRITE setLogLevel NOTIFY logLevelChanged)
        Q_PROPERTY(bool active READ active NOTIFY stateChanged)
        Q_PROPERTY(bool running READ running NOTIFY stateChanged)
        Q_PROPERTY(QString statusTitle READ statusTitle NOTIFY stateChanged)
        Q_PROPERTY(QString statusDetail READ statusDetail NOTIFY stateChanged)
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
        auto serverHost() const -> QString;
        auto serverPort() const -> int;
        auto clientHost() const -> QString;
        auto clientPort() const -> int;
        auto logLevel() const -> QString;
        auto active() const -> bool;
        auto running() const -> bool;
        auto statusTitle() const -> QString;
        auto statusDetail() const -> QString;
        auto logText() const -> QString;
        auto hasActiveScreen() const -> bool;
        auto activeScreenOwnerId() const -> QString;
        auto activeScreenIndex() const -> int;

        auto setSelectedMode(int mode) -> void;
        auto setServerHost(const QString &host) -> void;
        auto setServerPort(int port) -> void;
        auto setClientHost(const QString &host) -> void;
        auto setClientPort(int port) -> void;
        auto setLogLevel(const QString &level) -> void;

        Q_INVOKABLE void start(const QString &configPath);
        Q_INVOKABLE void stop();
        Q_INVOKABLE void clearLog();
        Q_INVOKABLE void quitApplication();

    signals:
        void selectedModeChanged();
        void serverHostChanged();
        void serverPortChanged();
        void clientHostChanged();
        void clientPortChanged();
        void logLevelChanged();
        void stateChanged();
        void logTextChanged();
        void activeScreenChanged();

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
        auto        finishRun() -> void;
        auto        appendLog(const QString &text) -> void;
        auto        setStatus(const QString &title, const QString &detail) -> void;
        auto        refreshActiveScreen() -> void;
        auto        clearRunningServer() -> void;
        static auto endpointText(const QString &host, int port) -> QString;

        ilias::StopHandle                    mRunHandle;
        std::stop_source                     mStopSource;
        RunMode                              mSelectedMode = ServerMode;
        RunState                             mState        = RunState::Idle;
        QString                              mServerHost;
        int                                  mServerPort = 24800;
        QString                              mClientHost;
        int                                  mClientPort = 24800;
        QString                              mLogLevel;
        QString                              mStatusTitle;
        QString                              mStatusDetail;
        QString                              mLogText;
        QString                              mRunError;
        QTimer                               mActiveScreenTimer;
        mks::Server                         *mRunningServer = nullptr;
        QString                              mActiveScreenOwnerId;
        int                                  mActiveScreenIndex = -1;
        bool                                 mStopping          = false;
        bool                                 mCompletedNormally = false;
        bool                                 mQuitWhenStopped   = false;
        std::shared_ptr<spdlog::sinks::sink> mLogSink;
    };

} // namespace mks::gui
