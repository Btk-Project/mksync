#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMouseEvent>
#include <QEnterEvent>
#include <QSizeGrip>
#include <QDebug>
#include <QScrollBar>
#include <QButtonGroup>

#ifdef _WIN32
    #include <Windows.h>
    #include <windowsx.h>
    #include <dwmapi.h>
#endif

#include "graphics_screen_item.hpp"
#include "mksync/base/default_configs.hpp"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), _settings("./config.json"), _ui(new Ui::MainWindow),
      _buttonGroup(new QButtonGroup(this))
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
}

MainWindow::~MainWindow()
{
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
        // 还原Windows系统窗口resize行为
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

void MainWindow::refresh_configs(QString file)
{
    if (!file.isEmpty()) {
        _settings.load(file.toStdString());
    }
    else {
        _settings.load();
    }

    _ui->config_file_edit->setText(QString::fromStdString(_settings.get_file().string()));
    _ui->screen_name_edit->setText(QString::fromStdString(
        _settings.get(mks::base::screen_name_config_name, mks::base::screen_name_default_value)));

    _ui->max_log_records_edit->setValue(_settings.get<int>(
        mks::base::max_log_records_config_name, mks::base::max_log_records_default_value));

    _ui->log_level_edit->setCurrentText(QString::fromStdString(
        _settings.get(mks::base::log_level_config_name, mks::base::log_level_default_value)));

    auto moduleList =
        _settings.get(mks::base::module_list_config_name, mks::base::module_list_default_value);
    QStringList moduleListQStr;
    for (auto &module : moduleList) {
        moduleListQStr << QString::fromStdString(module);
    }
    _ui->module_list_edit->setPlainText(moduleListQStr.join("\n"));

    auto remoteAddress = _settings.get(mks::base::remote_controller_config_name,
                                       mks::base::remote_controller_default_value);
    _ui->remote_address_edit->setText(QString::fromStdString(remoteAddress));

    auto serverIpaddress = _settings.get(mks::base::server_ipaddress_config_name,
                                         mks::base::server_ipaddress_default_value);
    auto serverIp        = serverIpaddress.substr(0, serverIpaddress.find_last_of(":"));
    auto serverPort      = serverIpaddress.substr(serverIpaddress.find_last_of(":") + 1);

    _ui->server_ip_edit->setText(QString::fromStdString(serverIp));
    _ui->server_port_edit->setValue(std::stoi(serverPort));

    _ui->client_ip_edit->setText(_ui->server_ip_edit->text());
    _ui->client_port_edit->setValue(_ui->server_port_edit->value());

    _scene.config_screens(
        _settings.get(mks::base::screen_name_config_name, mks::base::screen_name_default_value),
        _settings.get(mks::base::screen_settings_config_name,
                      mks::base::screen_settings_default_value));
    _ui->graphicsView->fit_view_to_scene();
}

void MainWindow::server_config()
{
    auto configs = _scene.get_screen_configs();
    _settings.set(mks::base::screen_settings_config_name, configs);
    auto ip   = _ui->server_ip_edit->text().toStdString();
    auto port = _ui->server_port_edit->text().toStdString();
    _settings.set(mks::base::server_ipaddress_config_name, ip + ":" + port);
    _settings.save();
}

void MainWindow::client_config()
{
    auto ip   = _ui->client_ip_edit->text().toStdString();
    auto port = _ui->client_port_edit->text().toStdString();
    _settings.set(mks::base::server_ipaddress_config_name, ip + ":" + port);
    _settings.save();
}

void MainWindow::server_start()
{
    // TODO:
}

void MainWindow::client_start()
{
    // TODO:
}

void MainWindow::setting_config()
{
    auto configFile = _ui->config_file_edit->text().toStdString();
    if (_settings.get_file() != std::filesystem::path(configFile)) {
        _settings.load(configFile);
    }

    auto screenName = _ui->screen_name_edit->text().toStdString();
    _settings.set(mks::base::screen_name_config_name, screenName);

    auto maxLogRecords = _ui->max_log_records_edit->value();
    _settings.set(mks::base::max_log_records_config_name, maxLogRecords);

    auto logLevel = _ui->log_level_edit->currentText().toStdString();
    _settings.set(mks::base::log_level_config_name, logLevel);

    std::vector<std::string> moduleList;
    for (const auto &module : _ui->module_list_edit->toPlainText().split("\n")) {
        if (!module.isEmpty()) {
            moduleList.push_back(module.toStdString());
        }
    }
    _settings.set(mks::base::module_list_config_name, moduleList);

    auto remoteAddress = _ui->remote_address_edit->text().toStdString();
    _settings.set(mks::base::remote_controller_config_name, remoteAddress);

    _settings.save();
}
