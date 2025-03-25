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

private:
    bool            _dragging;
    bool            _resizing;
    QPoint          _dragStartPos;
    QPoint          _resizeStartPos;
    QSize           _resizeStartSize;
    QGraphicsScene  _scene;
    Ui::MainWindow *_ui;
};

#endif // MAINWINDOW_H
