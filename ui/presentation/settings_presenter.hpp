#pragma once

#include "model/settings_model.hpp"
#include "preinclude.hpp"

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>

namespace mks::gui
{

    // Presenter is the sole interface used by QML. It converts the framework-free
    // Model into QML values and owns all configuration-related user actions.
    class SettingsPresenter final : public QObject {
        Q_OBJECT

        Q_PROPERTY(QString configPath READ configPath NOTIFY configPathChanged)
        Q_PROPERTY(QString machineId READ machineId NOTIFY machineIdChanged)
        Q_PROPERTY(QVariantList screens READ screens NOTIFY screensChanged)
        Q_PROPERTY(QVariantList trustedClients READ trustedClients NOTIFY trustedClientsChanged)
        Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
        Q_PROPERTY(bool hasError READ hasError NOTIFY statusMessageChanged)

    public:
        explicit SettingsPresenter(QObject *parent = nullptr);

        auto configPath() const -> QString;
        auto machineId() const -> QString;
        auto screens() const -> QVariantList;
        auto trustedClients() const -> QVariantList;
        auto statusMessage() const -> QString;
        auto hasError() const -> bool;

        Q_INVOKABLE void loadDefault();
        Q_INVOKABLE void loadConfigPath(const QString &path);
        Q_INVOKABLE void createNew();
        Q_INVOKABLE void importConfig(const QUrl &url);
        Q_INVOKABLE void save();
        Q_INVOKABLE void saveAs(const QUrl &url);
        Q_INVOKABLE void addScreen(const QString &ownerId, int screenIndex, int x, int y);
        Q_INVOKABLE void moveScreen(int row, int x, int y);
        Q_INVOKABLE void removeScreen(int row);
        Q_INVOKABLE void addTrustedClient(const QString &machineId, const QString &name);
        Q_INVOKABLE void removeTrustedClient(int row);

    signals:
        void configPathChanged();
        void machineIdChanged();
        void screensChanged();
        void trustedClientsChanged();
        void statusMessageChanged();

    private:
        auto publishConfig() -> void;
        auto showSuccess(const QString &message) -> void;
        auto showError(const QString &action, const std::error_code &error) -> void;
        auto showValidationError(const QString &message) -> void;
        auto cellIsOccupied(int x, int y, std::optional<size_t> ignoredRow = {}) const -> bool;
        static auto localPath(const QUrl &url) -> std::optional<std::filesystem::path>;

        SettingsModel mModel;
        QString       mStatusMessage;
        bool          mHasError = false;
    };

} // namespace mks::gui
