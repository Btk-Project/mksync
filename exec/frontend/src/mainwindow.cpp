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

#ifdef _WIN32
    #include <Windows.h>
    #include <windowsx.h>
    #include <dwmapi.h>
#endif

#include "graphics_screen_item.hpp"
#include "materialtoast.hpp"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), _settingFile(QApplication::applicationDirPath() + "/config.json"),
      _ui(new Ui::MainWindow), _buttonGroup(new QButtonGroup(this)), _rpcClient(_ctxt)
{
    _ui->setupUi(this);
    // 设置无边框窗口
    setWindowFlags(Qt::FramelessWindowHint);
    // 设置窗口透明
    setAttribute(Qt::WA_TranslucentBackground);
    HWND  hwnd  = (HWND)this->winId();
    DWORD style = ::GetWindowLong(hwnd, GWL_STYLE);
    ::SetWindowLong(hwnd, GWL_STYLE, style | WS_MAXIMIZEBOX | WS_THICKFRAME);
    connect(_ui->minimize_button, &QPushButton::clicked, this, &MainWindow::showMinimized);
    connect(_ui->layout_button, &QPushButton::clicked, this, [this]() {
        if (isMaximized()) {
            showNormal();
        }
        else {
            showMaximized();
        }
    });
    connect(_ui->close_button, &QPushButton::clicked, this, &MainWindow::close);

    connect(_ui->client_config_button, &QPushButton::clicked, this, &MainWindow::client_config);
    connect(_ui->server_config_button, &QPushButton::clicked, this, &MainWindow::server_config);
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

    connect(&_rpcReconnectTimer, &QTimer::timeout, this, &MainWindow::setup_backend);
    _rpcReconnectTimer.setSingleShot(true);
    _rpcReconnectTimer.start(3000);

#ifndef NDEBUG
    _ui->listWidget->addItem(ScreenListView::make_screen_item("testScreen0", QSize{1920, 1080}));
    _ui->listWidget->addItem(ScreenListView::make_screen_item("testScreen1", QSize{1440, 900}));
    _ui->listWidget->addItem(ScreenListView::make_screen_item("testScreen2", QSize{1280, 720}));
    _ui->listWidget->addItem(ScreenListView::make_screen_item("testScreen3", QSize{1024, 768}));
#endif
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
    // if (event->button() == Qt::LeftButton) {
    //     if (_ui->title->geometry().contains(event->pos())) {
    //         // 否则进入拖拽模式
    //         _dragging     = true;
    //         _dragStartPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
    //     }
    // }
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    // if (_dragging) {
    //     // 移动窗口
    //     move(event->globalPosition().toPoint() - _dragStartPos);
    // }
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    // if (event->button() == Qt::LeftButton) {
    //     if (_dragging) {
    //         unsetCursor(); // 恢复默认光标
    //     }
    //     _dragging = false;
    // }
}

void MainWindow::showEvent(QShowEvent *event)
{
    refresh_configs();
    QMainWindow::showEvent(event);
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef _WIN32
    MSG *msg = static_cast<MSG *>(message);
    switch (msg->message) {
    case WM_NCHITTEST: {
        *result               = 0;
        const int borderWidth = 8;
        RECT      winrect;
        GetWindowRect((HWND)winId(), &winrect);
        auto posx = GET_X_LPARAM(msg->lParam);
        auto posy = GET_Y_LPARAM(msg->lParam);

        // 还原windows系统snap assist行为
        double dpr = this->devicePixelRatioF();
        QPoint pos = _ui->title->mapFromGlobal(QPoint(posx / dpr, posy / dpr));

        if (_ui->title->rect().contains(pos)) {
            *result = HTCAPTION;
        }

        if (_ui->layout_button->geometry().contains(pos)) {
            *result = 0;
        }

        if (_ui->minimize_button->geometry().contains(pos)) {
            *result = 0;
        }

        if (_ui->close_button->geometry().contains(pos)) {
            *result = 0;
        }

        // 还原Windows系统窗口resize行为, 优先级高于标题栏拖拽行为，否则会导致标题栏上无法缩放。
        if (posy < winrect.top + borderWidth) {
            *result = HTTOP;
        }
        if (posy > winrect.bottom - borderWidth) {
            *result = HTBOTTOM;
        }
        if (posx < winrect.left + borderWidth) {
            *result = HTLEFT;
        }
        if (posx > winrect.right - borderWidth) {
            *result = HTRIGHT;
        }
        if (posy < winrect.top + borderWidth && posx < winrect.left + borderWidth) {
            *result = HTTOPLEFT;
        }
        if (posy < winrect.top + borderWidth && posx > winrect.right - borderWidth) {
            *result = HTTOPRIGHT;
        }
        if (posy > winrect.bottom - borderWidth && posx < winrect.left + borderWidth) {
            *result = HTBOTTOMLEFT;
        }
        if (posy > winrect.bottom - borderWidth && posx > winrect.right - borderWidth) {
            *result = HTBOTTOMRIGHT;
        }

        if (*result != 0) {
            return true;
        }
    } break;
    default:
        break;
    }
#endif
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
    _ui->screen_name_edit->setText(_settings["screen_name"].toString(QLatin1String("unknow")));

    _ui->max_log_records_edit->setValue(_settings["max_log_records"].toInt(1000));

    _ui->log_level_edit->setCurrentText(_settings["log_level"].toString(QLatin1String("warn")));

    auto        moduleList = _settings["module_list"].toArray();
    QStringList moduleListQStr;
    for (auto module : moduleList) {
        moduleListQStr << module.toString();
    }
    _ui->module_list_edit->setPlainText(moduleListQStr.join("\n"));
    auto remoteAddress =
        _settings["remote_controller"].toString(QLatin1String("tcp://127.0.0.1:8578"));
    _ui->remote_address_edit->setText(remoteAddress);

    auto serverIpaddress = _settings["server_ipaddress"].toString(QLatin1String("0.0.0.0:857"));

    auto serverIp   = serverIpaddress.mid(0, serverIpaddress.lastIndexOf(':'));
    auto serverPort = serverIpaddress.mid(serverIpaddress.lastIndexOf(':') + 1);

    _ui->server_ip_edit->setText(serverIp);
    _ui->server_port_edit->setValue(serverPort.toInt(0));

    _ui->client_ip_edit->setText(_ui->server_ip_edit->text());
    _ui->client_port_edit->setValue(_ui->server_port_edit->value());

    _scene.config_screens(_settings["screen_name"].toString(QLatin1String("unknow")),
                          _settings["screen_settings"].toArray());
    _ui->graphicsView->fit_view_to_scene();
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

void MainWindow::setup_backend()
{
    auto remoteAddress = _ui->remote_address_edit->text();
    if (!_rpcClient.isConnected()) {
        if (!(ilias_wait _rpcClient.connect(remoteAddress.toStdString())) &&
            _process.state() != QProcess::Running) {
            _process.startCommand(QString("MKsyncBackend config --dir=%1 --noconsole")
                                      .arg(_ui->config_file_edit->text()));
            _process.waitForStarted();
            ilias_wait _rpcClient.connect(remoteAddress.toStdString());
        }
    }
    if (_rpcClient.isConnected()) {
        if (auto ret = ilias_wait _rpcClient->clientStatus(); ret && ret.value() == 1) {
            _ui->client_start_button->setText(tr("stop"));
        }
        else {
            _ui->client_start_button->setText(tr("start"));
        }

        if (auto ret = ilias_wait _rpcClient->serverStatus(); ret && ret.value() == 1) {
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

void MainWindow::server_config()
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

    ilias_wait _rpcClient->reloadConfigFile(_settingFile.toStdString());
}

void MainWindow::client_config()
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

    ilias_wait _rpcClient->reloadConfigFile(_settingFile.toStdString());
}

void MainWindow::server_start()
{
    if (_ui->server_start_button->text() == tr("start")) {
        auto ip   = _ui->server_ip_edit->text();
        auto port = _ui->server_port_edit->text();
        if (ip.contains(':')) {
            ip = "[" + ip + "]";
        }

        _ui->server_start_button->setDisabled(true);
        if (auto ret = ilias_wait _rpcClient->server(mks::ServerControl::eStart, ip.toStdString(),
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
        ilias_wait _rpcClient->server(mks::ServerControl::eStop, "", 0);
        _ui->server_start_button->setText(tr("start"));
    }
}

void MainWindow::client_start()
{
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
        ilias_wait _rpcClient->client(mks::ClientControl::eStop, "", 0);
        _ui->client_start_button->setText(tr("start"));
    }
}

void MainWindow::setting_config()
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

    ilias_wait _rpcClient->reloadConfigFile(_settingFile.toStdString());
}
