#include "graphics_screen_item.hpp"

#include <QPainter>
#include <QGraphicsScene>
#include <QList>
#include <QGraphicsSceneMouseEvent>
#include <QDebug>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QApplication>
#include <QFontMetrics>

#include "screen_scene.hpp"
#include "detail/rect_algorithm.hpp"

GraphicsScreenItem::GraphicsScreenItem(const QString &screenName, const int &screenId,
                                       const QSize &itemSize)
    : _screenName(screenName), _screenId(screenId), _itemGeometry({0, 0}, itemSize)
{
    _path.addRect(0, 0, _itemGeometry.width(), _itemGeometry.height());
}

void GraphicsScreenItem::set_screen_name(const QString &screenName)
{
    _screenName = screenName;
}

void GraphicsScreenItem::set_screen_id(const int &screenId)
{
    _screenId = screenId;
}

void GraphicsScreenItem::set_item_geometry(const QRect &itemGeometry)
{
    _path.clear();
    _path.addRect(0, 0, itemGeometry.width(), itemGeometry.height());
    _itemGeometry = itemGeometry;
    setPos(_itemGeometry.topLeft());
}

QString GraphicsScreenItem::get_screen_name() const
{
    return _screenName;
}

int GraphicsScreenItem::get_screen_id() const
{
    return _screenId;
}

QRect GraphicsScreenItem::get_item_geometry() const
{
    return _itemGeometry;
}

QPainterPath GraphicsScreenItem::shape() const
{
    QPainterPath path;
    path.addRect(boundingRect().adjusted(1, 1, -1, -1));
    return path;
}

QRectF GraphicsScreenItem::boundingRect() const
{
    return QRectF(0, 0, _itemGeometry.width(), _itemGeometry.height());
}

void GraphicsScreenItem::paint(QPainter                                        *painter,
                               [[maybe_unused]] const QStyleOptionGraphicsItem *option,
                               [[maybe_unused]] QWidget                        *widget)
{
    if (painter == nullptr || !painter->isActive()) {
        return;
    }
    painter->save();
    // 如果z大于0，表示正在其他状态，画个高亮的框
    auto rect = boundingRect();
    painter->setPen(QColor("#EADDFF"));
    painter->drawRect(rect);
    // 绘制背景的矩形框
    if (zValue() > 0) {
        painter->setBrush(QColor("#6750A4"));
    }
    else {
        painter->setBrush(QColor("#544597"));
    }
    painter->drawRect(rect);
    // 绘制屏幕名称
    painter->setFont(_font);
    painter->setPen(Qt::black);
    painter->drawText(rect, Qt::AlignCenter, _showText);
    painter->restore();
}

void GraphicsScreenItem::dragMoveEvent(QGraphicsSceneDragDropEvent *event)
{
    return QGraphicsItem::dragMoveEvent(event);
}

auto GraphicsScreenItem::fit_font(const QTransform &transform) -> void
{
    setToolTip(_screenName); // 设置提示
    _font = QApplication::font();
    _font.setPixelSize(20 / transform.m11());
    // 计算文本长度，并确定省略部分
    auto fm        = QFontMetricsF(_font);
    auto textWidth = fm.horizontalAdvance(_screenName);
    if (textWidth > _itemGeometry.width()) {
        _showText = fm.elidedText(_screenName, Qt::ElideRight, _itemGeometry.width());
    }
    else {
        _showText = _screenName;
    }
}

void GraphicsScreenItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    setZValue(1);
    QGraphicsItem::mousePressEvent(event);
}

// 鼠标释放时处理网格占用和交换
void GraphicsScreenItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    setZValue(0);
    QGraphicsItem::mouseReleaseEvent(event);
    // 直接移动到目标位置
    update_grid_pos(pos());
    adsorption_to_grid();
}

void GraphicsScreenItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mouseMoveEvent(event);
}

void GraphicsScreenItem::update_grid_pos(const QPointF &scenePos)
{
    auto gridPos = adsorption_to_grid(scene(), this, scenePos, _itemGeometry.size());
    _gridPos     = gridPos;
}

void GraphicsScreenItem::adsorption_to_grid()
{
    setPos(_gridPos);
    _itemGeometry.moveTo(_gridPos);

    auto *scene = qobject_cast<ScreenScene *>(this->scene());
    if (scene != nullptr) {
        if (scene->collidingItems(this).size() >= 1) {
            Q_EMIT scene->remove_screen(
                mks::VirtualScreenConfig{.name   = get_screen_name().toStdString(),
                                         .posX   = 0,
                                         .posY   = 0,
                                         .width  = _itemGeometry.width(),
                                         .height = _itemGeometry.height()});
            scene->removeItem(this);
            this->deleteLater();
        }
    }
}

QPoint GraphicsScreenItem::adsorption_to_grid(QGraphicsScene *scene, QGraphicsItem *self,
                                              const QPointF &scenePos, const QSize &itemSize)
{
    auto      gridRect  = QRectF{scenePos, itemSize};
    const int threshold = 300;
    for (auto *item : scene->items()) {
        if (self == item) {
            continue;
        }
        auto rect = item->sceneBoundingRect();
        // 检测边侧距离
        // 如果当前的矩形靠近了目标矩形的边界，则自动对齐到目标边界。
        QRectF retRect  = gridRect;
        auto   overlap  = detail::is_rect_overlap(gridRect, rect);
        int    touching = 0;
        if (((overlap & 2) != 0) && std::abs(rect.left() - gridRect.right()) < threshold) {
            retRect.moveRight(rect.left());
            touching = 1;
        }
        else if (((overlap & 2) != 0) && std::abs(rect.right() - gridRect.left()) < threshold) {
            retRect.moveLeft(rect.right());
            touching = 1;
        }
        else if (((overlap & 1) != 0) && std::abs(rect.top() - gridRect.bottom()) < threshold) {
            retRect.moveBottom(rect.top());
            touching = 2;
        }
        else if (((overlap & 1) != 0) && std::abs(rect.bottom() - gridRect.top()) < threshold) {
            retRect.moveTop(rect.bottom());
            touching = 2;
        }
        auto citems = scene->items(retRect);
        if ((citems.size() == 1 && citems.back() != self) || citems.size() > 1) {
            continue;
        }
        if (touching == 1) {
            if (std::abs(rect.top() - gridRect.top()) < threshold) {
                retRect.moveTop(rect.top());
            }
            else if (std::abs(rect.bottom() - gridRect.bottom()) < threshold) {
                retRect.moveBottom(rect.bottom());
            }
        }
        if (touching == 2) {
            if (std::abs(rect.left() - gridRect.left()) < threshold) {
                retRect.moveLeft(rect.left());
            }
            else if (std::abs(rect.right() - gridRect.right()) < threshold) {
                retRect.moveRight(rect.right());
            }
        }
        citems = scene->items(retRect);
        if ((citems.size() == 1 && citems.back() != self) || citems.size() > 1) {
            continue;
        }
        gridRect = retRect;
    }
    return gridRect.topLeft().toPoint();
}