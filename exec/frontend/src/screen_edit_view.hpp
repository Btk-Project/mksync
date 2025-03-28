/**
 * @file screen_edit_view.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-03-26
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include <QGraphicsView>
#include <QPushButton>
#include <QWidget>
#include <QCheckBox>
#include <QLabel>

#include "graphics_screen_item.hpp"

class ScreenEditView : public QGraphicsView {
    Q_OBJECT

public:
    ScreenEditView(QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;

public Q_SLOTS:
    void location_button_clicked();
    void pushpin_button_clicked(bool checked);
    void fit_view_to_scene();
    void zoom_in_out_view(float scale);

private:
    void _update_view_area(QPointF pos);

private:
    int _horizontalValue = 0;
    int _verticalValue   = 0;

    bool _isPushpin        = false;
    bool _isItemMoving     = false;
    bool _isToolMenuMoving = false;

    GraphicsScreenItem *_selfItem;
    QPushButton        *_locationButton;
    QCheckBox          *_pushpinButton;
    QWidget            *_suspensionWidget;
    QPushButton        *_fitViewButton;
    QPushButton        *_zoomInButton;
    QPushButton        *_zoomOutButton;
    QLabel             *_dragPoint;
};