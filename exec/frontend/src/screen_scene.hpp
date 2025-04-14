/**
 * @file screen_scene.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-03-26
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include <QGraphicsScene>
#include <QJsonArray>

#include "graphics_screen_item.hpp"

class ScreenScene : public QGraphicsScene {
    Q_OBJECT
public:
    ScreenScene(QObject *parent = nullptr);
    ScreenScene(const QSize &screenSize, QObject *parent = nullptr);
    ~ScreenScene();

    auto get_screen_configs() const -> QJsonArray;
    auto config_screens(const QString &self, const QJsonArray &configs) -> void;
    auto get_self_item() -> GraphicsScreenItem *;
    auto contains_screen(const QString &screen) -> bool;
    auto new_screen_item(QString screen, QPoint scenePos, QSize size, int id = 0)
        -> GraphicsScreenItem *;
    bool remove_screen_item(QString screen);

Q_SIGNALS:
    void remove_screen(QString screen, QSize size);

protected:
    void drawBackground(QPainter *painter, const QRectF &rect) override;
    void dragEnterEvent(QGraphicsSceneDragDropEvent *event) override;
    void dragMoveEvent(QGraphicsSceneDragDropEvent *event) override;
    void dropEvent(QGraphicsSceneDragDropEvent *event) override;
    void dragLeaveEvent(QGraphicsSceneDragDropEvent *event) override;

private:
    GraphicsScreenItem *_selfItem = nullptr;
};