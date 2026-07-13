#include "presentation/settings_presenter.hpp"
#include "preinclude.hpp"

#include <QVariantMap>

#include <limits>

namespace mks::gui
{

    SettingsPresenter::SettingsPresenter(QObject *parent) : QObject(parent)
    {
        mModel.createNew();
        showSuccess(tr("新配置已就绪；保存后即可供命令行版本使用。"));
    }

    auto SettingsPresenter::configPath() const -> QString
    {
        return QString::fromStdString(mModel.path().string());
    }

    auto SettingsPresenter::machineId() const -> QString
    {
        return QString::fromStdString(mModel.config().machineId);
    }

    auto SettingsPresenter::screens() const -> QVariantList
    {
        auto result = QVariantList{};
        for (const auto &screen : mModel.config().screens) {
            auto entry = QVariantMap{};
            entry.insert(QStringLiteral("ownerId"), QString::fromStdString(screen.ownerId));
            entry.insert(QStringLiteral("screenIndex"), static_cast<int>(screen.screenIndex));
            entry.insert(QStringLiteral("x"), screen.cell.x);
            entry.insert(QStringLiteral("y"), screen.cell.y);
            entry.insert(QStringLiteral("isLocal"), screen.ownerId == mModel.config().machineId);
            result.append(entry);
        }
        return result;
    }

    auto SettingsPresenter::trustedClients() const -> QVariantList
    {
        auto result = QVariantList{};
        for (const auto &client : mModel.config().trustedClients) {
            auto entry = QVariantMap{};
            entry.insert(QStringLiteral("machineId"), QString::fromStdString(client.machineId));
            entry.insert(QStringLiteral("name"), QString::fromStdString(client.name));
            result.append(entry);
        }
        return result;
    }

    auto SettingsPresenter::statusMessage() const -> QString
    {
        return mStatusMessage;
    }

    auto SettingsPresenter::hasError() const -> bool
    {
        return mHasError;
    }

    auto SettingsPresenter::loadDefault() -> void
    {
        auto result = mModel.loadOrCreate(std::filesystem::path{"mksync.json"});
        if (!result) {
            showError(tr("无法载入默认配置"), result.error());
            return;
        }
        publishConfig();
        showSuccess(tr("已载入 %1").arg(configPath()));
    }

    auto SettingsPresenter::createNew() -> void
    {
        mModel.createNew();
        publishConfig();
        showSuccess(tr("已创建新配置。选择“另存为”以写入文件。"));
    }

    auto SettingsPresenter::importConfig(const QUrl &url) -> void
    {
        auto path = localPath(url);
        if (!path) {
            showValidationError(tr("只能导入本地配置文件。"));
            return;
        }

        auto result = mModel.importFrom(*path);
        if (!result) {
            showError(tr("导入失败"), result.error());
            return;
        }
        publishConfig();
        showSuccess(tr("已导入 %1").arg(configPath()));
    }

    auto SettingsPresenter::save() -> void
    {
        auto result = mModel.save();
        if (!result) {
            if (mModel.path().empty()) {
                showValidationError(tr("此配置尚未指定文件。请选择“另存为”。"));
            }
            else {
                showError(tr("保存失败"), result.error());
            }
            return;
        }
        publishConfig();
        showSuccess(tr("已保存到 %1").arg(configPath()));
    }

    auto SettingsPresenter::saveAs(const QUrl &url) -> void
    {
        auto path = localPath(url);
        if (!path) {
            showValidationError(tr("请选择本地保存位置。"));
            return;
        }

        auto result = mModel.saveAs(*path);
        if (!result) {
            showError(tr("另存为失败"), result.error());
            return;
        }
        publishConfig();
        showSuccess(tr("已保存到 %1").arg(configPath()));
    }

    auto SettingsPresenter::addScreen(const QString &ownerId, int screenIndex, int x, int y) -> void
    {
        const auto trimmedOwnerId = ownerId.trimmed();
        if (trimmedOwnerId.isEmpty()) {
            showValidationError(tr("设备 ID 不能为空。"));
            return;
        }
        if (screenIndex < 0) {
            showValidationError(tr("屏幕编号不能为负数。"));
            return;
        }

        const auto  ownerIdText = trimmedOwnerId.toStdString();
        auto        existingRow = std::optional<size_t>{};
        const auto &screens     = mModel.config().screens;
        for (size_t row = 0; row < screens.size(); ++row) {
            const auto &screen = screens[row];
            if (screen.ownerId == ownerIdText &&
                screen.screenIndex == static_cast<uint32_t>(screenIndex)) {
                existingRow = row;
                break;
            }
        }
        if (cellIsOccupied(x, y, existingRow)) {
            showValidationError(tr("该网格坐标已经被其他屏幕占用。"));
            return;
        }

        upsertScreenLayout(mModel.config(), ScreenLayoutConfig{
                                                .ownerId     = ownerIdText,
                                                .screenIndex = static_cast<uint32_t>(screenIndex),
                                                .cell        = GridPosition{.x = x, .y = y},
        });
        publishConfig();
        showSuccess(tr("屏幕布局已更新。"));
    }

    auto SettingsPresenter::moveScreen(int row, int x, int y) -> void
    {
        auto &screens = mModel.config().screens;
        if (row < 0 || static_cast<size_t>(row) >= screens.size()) {
            showValidationError(tr("找不到要移动的屏幕。"));
            return;
        }
        if (cellIsOccupied(x, y, static_cast<size_t>(row))) {
            showValidationError(tr("该网格坐标已经被其他屏幕占用。"));
            return;
        }
        screens[static_cast<size_t>(row)].cell = GridPosition{.x = x, .y = y};
        publishConfig();
        showSuccess(tr("屏幕位置已更新；记得保存配置。"));
    }

    auto SettingsPresenter::removeScreen(int row) -> void
    {
        auto &screens = mModel.config().screens;
        if (row < 0 || static_cast<size_t>(row) >= screens.size()) {
            showValidationError(tr("找不到要移除的屏幕。"));
            return;
        }
        screens.erase(screens.begin() + row);
        publishConfig();
        showSuccess(tr("已移除屏幕布局。"));
    }

    auto SettingsPresenter::addTrustedClient(const QString &machineId, const QString &name) -> void
    {
        const auto trimmedMachineId = machineId.trimmed();
        const auto trimmedName      = name.trimmed();
        if (trimmedMachineId.isEmpty() && trimmedName.isEmpty()) {
            showValidationError(tr("请输入设备 ID 或显示名称。"));
            return;
        }

        mModel.config().trustedClients.push_back(TrustedClientConfig{
            .machineId = trimmedMachineId.toStdString(),
            .name      = trimmedName.toStdString(),
        });
        publishConfig();
        showSuccess(tr("已添加可信设备。"));
    }

    auto SettingsPresenter::removeTrustedClient(int row) -> void
    {
        auto &clients = mModel.config().trustedClients;
        if (row < 0 || static_cast<size_t>(row) >= clients.size()) {
            showValidationError(tr("找不到要移除的可信设备。"));
            return;
        }
        clients.erase(clients.begin() + row);
        publishConfig();
        showSuccess(tr("已移除可信设备。"));
    }

    auto SettingsPresenter::publishConfig() -> void
    {
        emit configPathChanged();
        emit machineIdChanged();
        emit screensChanged();
        emit trustedClientsChanged();
    }

    auto SettingsPresenter::showSuccess(const QString &message) -> void
    {
        mStatusMessage = message;
        mHasError      = false;
        emit statusMessageChanged();
    }

    auto SettingsPresenter::showError(const QString &action, const std::error_code &error) -> void
    {
        mStatusMessage = tr("%1：%2").arg(action, QString::fromStdString(error.message()));
        mHasError      = true;
        emit statusMessageChanged();
    }

    auto SettingsPresenter::showValidationError(const QString &message) -> void
    {
        mStatusMessage = message;
        mHasError      = true;
        emit statusMessageChanged();
    }

    auto SettingsPresenter::cellIsOccupied(int x, int y, std::optional<size_t> ignoredRow) const
        -> bool
    {
        const auto &screens = mModel.config().screens;
        for (size_t row = 0; row < screens.size(); ++row) {
            if (ignoredRow && row == *ignoredRow) {
                continue;
            }
            if (screens[row].cell == (GridPosition{.x = x, .y = y})) {
                return true;
            }
        }
        return false;
    }

    auto SettingsPresenter::localPath(const QUrl &url) -> std::optional<std::filesystem::path>
    {
        if (!url.isLocalFile()) {
            return std::nullopt;
        }
        return std::filesystem::path{url.toLocalFile().toStdString()};
    }

} // namespace mks::gui
