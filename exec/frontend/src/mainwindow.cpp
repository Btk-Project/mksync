#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMouseEvent>
#include <QEnterEvent>
#include <QSizeGrip>
#include <QDebug>
#include <QScrollBar>
#include <QButtonGroup>
#include <QFile>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

#include "graphics_screen_item.hpp"
#include "materialtoast.hpp"
#include "mksync/base/default_configs.hpp"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), _settingFile(QApplication::applicationDirPath() + "/config.json"),
      _ui(new Ui::MainWindow), _buttonGroup(new QButtonGroup(this)), _rpcClient(_ctxt)
{
    _ui->setupUi(this);

    connect(_ui->server_start_button, &QPushButton::clicked, this, &MainWindow::server_start);
    connect(_ui->client_start_button, &QPushButton::clicked, this, &MainWindow::client_start);
    connect(_ui->setting_config_button, &QPushButton::clicked, this, &MainWindow::setting_config);

    _buttonGroup->addButton(_ui->server);
    _buttonGroup->addButton(_ui->client);
    _buttonGroup->addButton(_ui->help);
    _buttonGroup->addButton(_ui->setting);
    _buttonGroup->setExclusive(true);
    connect(_buttonGroup, &QButtonGroup::buttonClicked, this, &MainWindow::button_clicked);

    _ui->server->setChecked(true);
    _ui->stackedWidget->setCurrentWidget(_ui->server_page);
    _ui->graphicsView->setScene(&_scene);

    connect(&_scene, &ScreenScene::remove_screen, this, [this](QString screen, QSize size) {
        auto *item = ScreenListView::make_screen_item(screen, size);
        _ui->listWidget->addItem(item);
    });
    connect(_ui->graphicsView, &ScreenEditView::refresh_screens_request, this,
            &MainWindow::refresh_online_screens);
    connect(&_rpcReconnectTimer, &QTimer::timeout, this, &MainWindow::setup_backend);
    _rpcReconnectTimer.setSingleShot(true);
    _rpcReconnectTimer.start(10);
}

MainWindow::~MainWindow()
{
    _process.kill();
    _process.waitForFinished();
    _rpcClient.close();
    delete _ui;
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    return QMainWindow::mousePressEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    return QMainWindow::mouseMoveEvent(event);
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    return QMainWindow::mouseReleaseEvent(event);
}

void MainWindow::showEvent(QShowEvent *event)
{
    refresh_configs();
    QMainWindow::showEvent(event);
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::button_clicked(QAbstractButton *button)
{
    if (button == _ui->server) {
        _ui->stackedWidget->setCurrentWidget(_ui->server_page);
    }
    else if (button == _ui->client) {
        _ui->stackedWidget->setCurrentWidget(_ui->client_page);
    }
    else if (button == _ui->setting) {
        _ui->stackedWidget->setCurrentWidget(_ui->setting_page);
    }
    else if (button == _ui->help) {
        _ui->stackedWidget->setCurrentWidget(_ui->help_page);
    }
}
void MainWindow::_load_configs(const QString &file)
{
    if (!file.isEmpty()) {
        QFile infile(file);
        if (!infile.open(QIODevice::ReadOnly)) {
            MaterialToast::show(this, infile.errorString());
            return;
        }
        QJsonParseError error;
        auto            parser = QJsonDocument::fromJson(infile.readAll(), &error);
        if (error.error == QJsonParseError::NoError && !parser.isEmpty()) {
            _settingFile = file;
            _settings    = parser.object();
        }
        else {
            MaterialToast::show(
                this, error.error == QJsonParseError::NoError ? "空配置" : error.errorString());
        }
    }
    else {
        QFile infile(_settingFile);
        if (!infile.open(QIODevice::ReadOnly)) {
            MaterialToast::show(this, infile.errorString());
            return;
        }
        QJsonParseError error;
        auto            parser = QJsonDocument::fromJson(infile.readAll(), &error);
        if (error.error == QJsonParseError::NoError && !parser.isEmpty()) {
            _settings = parser.object();
        }
        else {
            MaterialToast::show(
                this, error.error == QJsonParseError::NoError ? "空配置" : error.errorString());
        }
    }
}

void MainWindow::refresh_configs(QString file)
{
    _load_configs(file);
    _ui->config_file_edit->setText(_settingFile);
    _ui->screen_name_edit->setText(_settings[mks::screen_name_config_name].toString(
        QString::fromStdString(mks::screen_name_default_value)));

    _ui->max_log_records_edit->setValue(
        _settings[mks::max_log_records_config_name].toInt(mks::max_log_records_default_value));

    _ui->log_level_edit->setCurrentText(_settings[mks::log_level_config_name].toString(
        QString::fromStdString(mks::log_level_default_value)));

    auto        moduleList = _settings[mks::module_list_config_name].toArray();
    QStringList moduleListQStr;
    for (auto module : moduleList) {
        moduleListQStr << module.toString();
    }
    _ui->module_list_edit->setPlainText(moduleListQStr.join("\n"));
    auto remoteAddress = _settings[mks::remote_controller_config_name].toString(
        QString::fromStdString(mks::remote_controller_default_value));
    _ui->remote_address_edit->setText(remoteAddress);

    auto serverIpaddress = _settings[mks::server_ipaddress_config_name].toString(
        QString::fromStdString(mks::server_ipaddress_default_value));

    auto serverIp   = serverIpaddress.mid(0, serverIpaddress.lastIndexOf(':'));
    auto serverPort = serverIpaddress.mid(serverIpaddress.lastIndexOf(':') + 1);

    _ui->server_ip_edit->setText(serverIp);
    _ui->server_port_edit->setValue(serverPort.toInt(0));

    _ui->client_ip_edit->setText(_ui->server_ip_edit->text());
    _ui->client_port_edit->setValue(_ui->server_port_edit->value());

    _scene.config_screens(_ui->screen_name_edit->text(),
                          _settings[mks::screen_settings_config_name].toArray());
    _ui->graphicsView->fit_view_to_scene();
}

auto MainWindow::refresh_online_screens() -> ::ilias::QAsyncSlot<void>
{
    auto ret = co_await _rpcClient->getOnlineScreens();
    if (ret) {
        auto                                                 screens = std::move(ret.value());
        std::map<std::string_view, mks::VirtualScreenInfo *> screenMap;
        for (auto &screen : screens) {
            screenMap[screen.name] = &screen;
        }
        auto screenInScene = _scene.items();
        for (auto *screen : screenInScene) {
            auto *screenItem = dynamic_cast<GraphicsScreenItem *>(screen);
            if (auto item = screenMap.find(screenItem->get_screen_name().toStdString());
                item != screenMap.end()) {
                screenItem->set_screen_id(item->second->screenId);
                screenItem->update_size(QSize{(int)item->second->width, (int)item->second->height});
                screenItem->update_online(true);
                screenMap.erase(item);
            }
            else if (screenItem != _scene.get_self_item()) {
                screenItem->update_online(false);
            }
        }
        // 清理list里原有的
        _ui->listWidget->clear();
        for (auto [screenName, screen] : screenMap) {
            // 检查屏幕是否已经存在
            auto *item = ScreenListView::make_screen_item(QString::fromStdString(screen->name),
                                                          QSize(screen->width, screen->height));
            item->setData(ScreenListView::eScreenIdRole, screen->screenId);
            _ui->listWidget->addItem(item);
        }
    }
    else {
        MaterialToast::show(
            this,
            QString("获取在线屏幕失败: %1").arg(QString::fromStdString(ret.error().message())));
    }
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange) {
        if (isMaximized() || isFullScreen()) {
            if (!_isFull) {
                _isFull = true;
                _ui->centralWidget->setStyleSheet("QWidget {background-color: #302c54;}");
            }
        }
        else if (_isFull) {
            _isFull = false;
            _ui->centralWidget->setStyleSheet("QWidget {background-color: transparent;}");
        }
    }
}

void MainWindow::stop_rpc_reconnected_timer()
{
    _rpcReconnectTimer.stop();
}

void MainWindow::start_rpc_reconnected_timer()
{
    _rpcReconnectTimer.start(10000);
    _rpcReconnectTimer.setSingleShot(true);
}

auto MainWindow::setup_backend() -> ::ilias::QAsyncSlot<void>
{
    auto remoteAddress = _ui->remote_address_edit->text();
    if (!_rpcClient.isConnected()) {
        if (!(co_await _rpcClient.connect(remoteAddress.toStdString())) &&
            _process.state() != QProcess::Running) {
            _process.startCommand(QString("MKsyncBackend config --dir=%1 --noconsole")
                                      .arg(_ui->config_file_edit->text()));
            _process.waitForStarted();
            co_await _rpcClient.connect(remoteAddress.toStdString());
        }
    }
    if (_rpcClient.isConnected()) {
        if (auto ret = co_await _rpcClient->clientStatus(); ret && ret.value() == 1) {
            _ui->client_start_button->setText(tr("stop"));
        }
        else {
            _ui->client_start_button->setText(tr("start"));
        }

        if (auto ret = co_await _rpcClient->serverStatus(); ret && ret.value() == 1) {
            _ui->server_start_button->setText(tr("stop"));
        }
        else {
            _ui->server_start_button->setText(tr("start"));
        }
        stop_rpc_reconnected_timer();
    }
    else {
        MaterialToast::show(this, "backend程序连接失败");
        start_rpc_reconnected_timer();
    }
}

auto MainWindow::server_config() -> ::ilias::QAsyncSlot<void>
{
    auto configs                 = _scene.get_screen_configs();
    _settings["screen_settings"] = configs;
    auto ip                      = _ui->server_ip_edit->text();
    auto port                    = _ui->server_port_edit->text();
    if (ip.contains(':')) {
        ip = "[" + ip + "]";
    }
    _settings["server_ipaddress"] = ip + ":" + port;
    QFile file(_settingFile);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QJsonDocument modifiedDoc(_settings); // 用修改后的 QJsonObject 创建新文档
        file.write(modifiedDoc.toJson());
        file.close();
    }
    auto str = _settingFile.toStdString();
    co_await _rpcClient->reloadConfigFile(str);
    // co_return;
}

auto MainWindow::client_config() -> ::ilias::QAsyncSlot<void>
{
    auto ip   = _ui->client_ip_edit->text();
    auto port = _ui->client_port_edit->text();
    if (ip.contains(':')) {
        ip = "[" + ip + "]";
    }
    _settings["server_ipaddress"] = ip + ":" + port;
    QFile file(_settingFile);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QJsonDocument modifiedDoc(_settings); // 用修改后的 QJsonObject 创建新文档
        file.write(modifiedDoc.toJson());
        file.close();
    }

    co_await _rpcClient->reloadConfigFile(_settingFile.toStdString());
}

auto MainWindow::server_start() -> ::ilias::QAsyncSlot<void>
{
    co_await server_config();
    if (_ui->server_start_button->text() == tr("start")) {
        auto ip   = _ui->server_ip_edit->text();
        auto port = _ui->server_port_edit->text();
        if (ip.contains(':')) {
            ip = "[" + ip + "]";
        }

        _ui->server_start_button->setDisabled(true);
        if (auto ret = co_await _rpcClient->server(mks::ServerControl::eStart, ip.toStdString(),
                                                   port.toInt());
            ret && ret.value() == "") {
            _ui->server_start_button->setText(tr("stop"));
        }
        else {
            MaterialToast::show(
                this, QString("启动失败: %1")
                          .arg(QString::fromStdString(ret ? ret.value() : ret.error().message())));
        }
        _ui->server_start_button->setDisabled(false);
    }
    else if (_ui->server_start_button->text() == tr("stop")) {
        co_await _rpcClient->server(mks::ServerControl::eStop, "", 0);
        _ui->server_start_button->setText(tr("start"));
    }
}

auto MainWindow::client_start() -> ::ilias::QAsyncSlot<void>
{
    co_await client_config();
    if (_ui->client_start_button->text() == tr("start")) {
        auto ip   = _ui->client_ip_edit->text();
        auto port = _ui->client_port_edit->text();
        if (ip.contains(':')) {
            ip = "[" + ip + "]";
        }
        _ui->client_start_button->setDisabled(true);
        if (auto ret = ilias_wait _rpcClient->client(mks::ClientControl::eStart, ip.toStdString(),
                                                     port.toInt());
            ret && ret.value() == "") {
            _ui->client_start_button->setText(tr("stop"));
        }
        else {
            MaterialToast::show(
                this, QString("启动失败: %1")
                          .arg(QString::fromStdString(ret ? ret.value() : ret.error().message())));
        }
        _ui->client_start_button->setDisabled(false);
    }
    else if (_ui->client_start_button->text() == tr("stop")) {
        co_await _rpcClient->client(mks::ClientControl::eStop, "", 0);
        _ui->client_start_button->setText(tr("start"));
    }
}

auto MainWindow::setting_config() -> ::ilias::QAsyncSlot<void>
{
    auto configFile = _ui->config_file_edit->text();
    if (_settingFile != configFile) {
        _load_configs(configFile);
    }

    auto screenName          = _ui->screen_name_edit->text();
    _settings["screen_name"] = screenName;

    auto maxLogRecords           = _ui->max_log_records_edit->value();
    _settings["max_log_records"] = maxLogRecords;

    auto logLevel          = _ui->log_level_edit->currentText();
    _settings["log_level"] = logLevel;

    QJsonArray moduleList;
    for (const auto &module : _ui->module_list_edit->toPlainText().split("\n")) {
        if (!module.isEmpty()) {
            moduleList.push_back(module);
        }
    }
    _settings["module_list"] = moduleList;

    auto remoteAddress             = _ui->remote_address_edit->text();
    _settings["remote_controller"] = remoteAddress;

    QFile file(_settingFile);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QJsonDocument modifiedDoc(_settings); // 用修改后的 QJsonObject 创建新文档
        file.write(modifiedDoc.toJson());
        file.close();
    }

    co_await _rpcClient->reloadConfigFile(_settingFile.toStdString());
}
