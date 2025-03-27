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

#include "mksync/proto/config_proto.hpp"
#include "graphics_screen_item.hpp"

class ScreenScene : public QGraphicsScene {
    Q_OBJECT
public:
    ScreenScene(QObject *parent = nullptr);
    ScreenScene(const QSize &screenSize, QObject *parent = nullptr);
    ~ScreenScene();

    auto get_screen_configs() const -> std::vector<mks::VirtualScreenConfig>;
    auto config_screens(const std::string                           &self,
                        const std::vector<mks::VirtualScreenConfig> &configs) -> void;
    auto get_self_item() -> GraphicsScreenItem *;

protected:
    void drawBackground(QPainter *painter, const QRectF &rect) override;
    void dragEnterEvent(QGraphicsSceneDragDropEvent *event) override;
    void dragMoveEvent(QGraphicsSceneDragDropEvent *event) override;
    void dropEvent(QGraphicsSceneDragDropEvent *event) override;
    void dragLeaveEvent(QGraphicsSceneDragDropEvent *event) override;

private:
    GraphicsScreenItem *_selfItem = nullptr;
};