#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMouseEvent>
#include <QEnterEvent>
#include <QSizeGrip>
#include <QDebug>

#ifdef _WIN32
    #include <Windows.h>
    #include <windowsx.h>
#endif

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), _ui(new Ui::MainWindow)
{
    _ui->setupUi(this);
    // 设置无边框窗口
    setWindowFlags(Qt::FramelessWindowHint);
    // 设置窗口透明
    setAttribute(Qt::WA_TranslucentBackground);
    HWND  hwnd  = (HWND)this->winId();
    DWORD style = ::GetWindowLong(hwnd, GWL_STYLE);
    ::SetWindowLong(hwnd, GWL_STYLE,
                    style | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_THICKFRAME | WS_SYSMENU);
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

    connect(_ui->client_config_button, &QPushButton::clicked, this, []() {
        // TODO:
    });
    connect(_ui->server_config_button, &QPushButton::clicked, this, []() {
        // TODO:
    });
    connect(_ui->server_start_button, &QPushButton::clicked, this, []() {
        // TODO:
    });
    connect(_ui->client_start_button, &QPushButton::clicked, this, []() {
        // TODO:
    });

    connect(_ui->server, &QPushButton::clicked, this, [this](bool checked) {
        if (checked) {
            _ui->stackedWidget->setCurrentWidget(_ui->server_page);
        }
    });
    connect(_ui->client, &QPushButton::clicked, this, [this](bool checked) {
        if (checked) {
            _ui->stackedWidget->setCurrentWidget(_ui->client_page);
        }
    });
    connect(_ui->help, &QPushButton::clicked, this, [this](bool checked) {
        if (checked) {
            _ui->stackedWidget->setCurrentWidget(_ui->help_page);
        }
    });
    connect(_ui->setting, &QPushButton::clicked, this, [this](bool checked) {
        if (checked) {
            _ui->stackedWidget->setCurrentWidget(_ui->setting_page);
        }
    });
    _ui->server->setChecked(true);
    _ui->stackedWidget->setCurrentWidget(_ui->server_page);

    _ui->graphicsView->setScene(&_scene);

    QGraphicsWidget     *mainWidget = new QGraphicsWidget;
    QGraphicsGridLayout *layout     = new QGraphicsGridLayout(mainWidget);
    mainWidget->setLayout(layout);
    _ui->graphicsView->setAlignment(Qt::AlignCenter);

    // 添加文本项
    QGraphicsTextItem *textItem = new QGraphicsTextItem("Hello, QGraphicsWidget!", mainWidget);

    // // 嵌入 QPushButton
    // QPushButton          *button = new QPushButton("Click Me");
    // QGraphicsProxyWidget *proxy  = new QGraphicsProxyWidget;
    // proxy->setWidget(button);
    // layout->addItem(proxy);

    // 设置主容器属性
    mainWidget->setGeometry(50, 50, 300, 200);
    mainWidget->setAutoFillBackground(true);
    mainWidget->setPalette(QPalette(Qt::lightGray));

    _scene.addItem(mainWidget);
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
