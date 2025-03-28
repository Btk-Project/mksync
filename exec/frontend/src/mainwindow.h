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

#include "screen_scene.hpp"
#include "graphics_screen_item.hpp"
#include "mksync/base/settings.hpp"

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

public Q_SLOTS:
    void server_config();
    void client_config();
    void server_start();
    void client_start();
    void setting_config();
    void button_clicked(QAbstractButton *button);
    void refresh_configs(QString file = "");

private:
    bool                _dragging;
    bool                _resizing;
    QPoint              _dragStartPos;
    QPoint              _resizeStartPos;
    QSize               _resizeStartSize;
    ScreenScene         _scene;
    bool                _isFull;
    mks::base::Settings _settings;
    Ui::MainWindow     *_ui;
    QButtonGroup       *_buttonGroup;
};

#endif // MAINWINDOW_H
