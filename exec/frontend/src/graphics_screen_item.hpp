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
#include <QGraphicsTextItem>
#include <QFont>
#include <QGraphicsSceneContextMenuEvent>

class GraphicsScreenItem : public QGraphicsTextItem {
    Q_OBJECT
public:
    enum State
    {
        eOffline = 0,
        eOnline  = 1,

        eReachable   = 4,
        eUnreachable = 8
    };

public:
    GraphicsScreenItem()  = default;
    ~GraphicsScreenItem() = default;
    GraphicsScreenItem(const QString &screenName, const int &screenId, const QSize &itemSize);
    void    set_screen_name(const QString &screenName);
    void    set_screen_id(const int &screenId);
    void    set_item_geometry(const QRect &itemGeometry);
    void    update_size(const QSize &itemSize);
    QString get_screen_name() const;
    int     get_screen_id() const;
    QRect   get_item_geometry() const;
    auto    edit() -> void;
    auto    after_edit() -> void;
    auto    update_online(bool isOnline) -> void;
    auto    set_state(int states) -> void;

    QPainterPath shape() const override;
    QRectF       boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    // 鼠标释放时处理网格占用和交换
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void dragMoveEvent(QGraphicsSceneDragDropEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

    auto          update_reachable(bool isReachable) -> void;
    auto          fit_font(const QTransform &transform) -> void;
    void          update_grid_pos(const QPointF &scenePos);
    bool          adsorption_to_grid();
    static QPoint adsorption_to_grid(QGraphicsScene *scene, QGraphicsItem *self,
                                     const QPointF &scenePos, const QSize &itemSize);

Q_SIGNALS:
    void name_changed(const QString &oldname, const QString &newname);

private:
    QString      _oldScreenName;
    int          _screenId;
    QSize        _itemSize;
    QPainterPath _path;
    QPoint       _gridPos;
    int          _states = 0;
};