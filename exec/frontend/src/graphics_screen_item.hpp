/**
 * @file graphics_screen_item.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-03-26
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include <QGraphicsObject>
#include <QFont>

class GraphicsScreenItem : public QGraphicsObject {
    Q_OBJECT
public:
    GraphicsScreenItem()  = default;
    ~GraphicsScreenItem() = default;
    GraphicsScreenItem(const QString &screenName, const int &screenId, const QSize &itemSize);
    void    set_screen_name(const QString &screenName);
    void    set_screen_id(const int &screenId);
    void    set_item_geometry(const QRect &itemGeometry);
    QString get_screen_name() const;
    int     get_screen_id() const;
    QRect   get_item_geometry() const;

    QPainterPath shape() const override;
    QRectF       boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    // 鼠标释放时处理网格占用和交换
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void dragMoveEvent(QGraphicsSceneDragDropEvent *event) override;

    auto          fit_font(const QTransform &transform) -> void;
    void          update_grid_pos(const QPointF &scenePos);
    void          adsorption_to_grid();
    static QPoint adsorption_to_grid(QGraphicsScene *scene, QGraphicsItem *self,
                                     const QPointF &scenePos, const QSize &itemSize);

private:
    QString      _screenName;
    QString      _showText;
    QFont        _font;
    int          _screenId;
    QRect        _itemGeometry;
    QPainterPath _path;
    QPoint       _gridPos;
};