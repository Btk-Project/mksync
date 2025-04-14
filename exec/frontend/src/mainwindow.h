#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsWidget>
#include <QGraphicsLinearLayout>
#include <QGraphicsTextItem>
#include <QGraphicsProxyWidget>
#include <QGraphicsGridLayout>
#include <QPushButton>
#include <QJsonObject>
#include <ilias/ilias.hpp>
#include <ilias/platform/qt.hpp>
#include <ilias/platform/qt_utils.hpp>
#include <nekoproto/jsonrpc/jsonrpc.hpp>
#include <QProcess>
#include <QTimer>

#include "screen_scene.hpp"
#include "graphics_screen_item.hpp"
#include "mksync/proto/rpc_proto.hpp"

namespace Ui
{
    class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    void showEvent(QShowEvent *event) override;
    void changeEvent(QEvent *event) override;

    void stop_rpc_reconnected_timer();
    void start_rpc_reconnected_timer();
    [[nodiscard("coroutine function")]]
    auto refresh_online_screens() -> ::ilias::QAsyncSlot<void>;
    [[nodiscard("coroutine function")]]
    auto setup_backend() -> ::ilias::QAsyncSlot<void>;
    [[nodiscard("coroutine function")]]
    auto server_config() -> ::ilias::QAsyncSlot<void>;
    [[nodiscard("coroutine function")]]
    auto client_config() -> ::ilias::QAsyncSlot<void>;
    [[nodiscard("coroutine function")]]
    auto server_start() -> ::ilias::QAsyncSlot<void>;
    [[nodiscard("coroutine function")]]
    auto client_start() -> ::ilias::QAsyncSlot<void>;
    [[nodiscard("coroutine function")]]
    auto setting_config() -> ::ilias::QAsyncSlot<void>;
    void button_clicked(QAbstractButton *button);
    void refresh_configs(QString file = "");

private:
    void _load_configs(const QString &file);

private:
    bool                                       _dragging;
    bool                                       _resizing;
    QPoint                                     _dragStartPos;
    QPoint                                     _resizeStartPos;
    QSize                                      _resizeStartSize;
    ScreenScene                                _scene;
    bool                                       _isFull;
    QString                                    _settingFile;
    QJsonObject                                _settings;
    Ui::MainWindow                            *_ui;
    QButtonGroup                              *_buttonGroup;
    NekoProto::JsonRpcClient<mks::BaseMethods> _rpcClient;
    QTimer                                     _rpcReconnectTimer;
    QProcess                                   _process;
    ilias::QIoContext                          _ctxt;
};

#endif // MAINWINDOW_H
