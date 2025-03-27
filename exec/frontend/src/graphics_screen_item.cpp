#include "graphics_screen_item.hpp"

#include <QPainter>
#include <QGraphicsScene>
#include <QList>
#include <QGraphicsSceneMouseEvent>
#include <QDebug>
#include <QFontMetrics>
#include <QFontMetricsF>

#include "screen_scene.hpp"

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
    path.addRect(boundingRect());
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
    // 绘制背景的矩形框
    painter->setBrush(QColor("#6750A4"));
    painter->setPen(Qt::black);
    painter->drawRect(boundingRect());
    // 绘制屏幕名称
    auto font = painter->font();
    font.setPixelSize(std::min(_itemGeometry.width(), _itemGeometry.height()) / 4);
    painter->setFont(font);
    painter->setPen(Qt::black);
    painter->drawText(boundingRect(), Qt::AlignCenter, _screenName);
    painter->restore();
}

void GraphicsScreenItem::dragMoveEvent(QGraphicsSceneDragDropEvent *event)
{
    return QGraphicsItem::dragMoveEvent(event);
}

void GraphicsScreenItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mousePressEvent(event);
}

// 鼠标释放时处理网格占用和交换
void GraphicsScreenItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
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
}

QPoint GraphicsScreenItem::adsorption_to_grid(QGraphicsScene *scene, QGraphicsItem *self,
                                              const QPointF &scenePos, const QSize &itemSize)
{
    auto      gridRect  = QRectF{scenePos, itemSize};
    const int threshold = 300;
    int       top       = std::numeric_limits<int>::max();
    int       left      = std::numeric_limits<int>::max();
    int       bottom    = std::numeric_limits<int>::max();
    int       right     = std::numeric_limits<int>::max();
    for (auto *item : scene->items()) {
        if (self == item) {
            continue;
        }
        auto rect = item->sceneBoundingRect();
        // 检测边侧距离
        // 如果当前的矩形靠近了目标矩形的边界，则自动对齐到目标边界。
        if (gridRect.bottom() - rect.top() > threshold &&
            rect.bottom() - gridRect.top() > threshold &&
            std::abs(rect.left() - gridRect.right()) < threshold) {
            right = std::abs(rect.left()) < std::abs(right) ? rect.left() : right;
        }
        else if (gridRect.bottom() - rect.top() > threshold &&
                 rect.bottom() - gridRect.top() > threshold &&
                 std::abs(rect.right() - gridRect.left()) < threshold) {
            left = std::abs(rect.right()) < std::abs(left) ? rect.right() : left;
        }
        if (std::abs(rect.top() - gridRect.top()) < threshold) {
            top = std::abs(rect.top()) < std::abs(top) ? rect.top() : top;
        }
        else if (std::abs(rect.bottom() - gridRect.bottom()) < threshold) {
            bottom = std::abs(rect.bottom()) < std::abs(bottom) ? rect.bottom() : bottom;
        }
        if (gridRect.right() - rect.left() > threshold &&
            rect.right() - gridRect.left() > threshold &&
            std::abs(rect.top() - gridRect.bottom()) < threshold) {
            bottom = std::abs(rect.top()) < std::abs(bottom) ? rect.top() : bottom;
        }
        else if (gridRect.right() - rect.left() > threshold &&
                 rect.right() - gridRect.left() > threshold &&
                 std::abs(rect.bottom() - gridRect.top()) < threshold) {
            top = std::abs(rect.bottom()) < std::abs(top) ? rect.bottom() : top;
        }
        if (std::abs(rect.left() - gridRect.left()) < threshold) {
            left = std::abs(rect.left()) < std::abs(left) ? rect.left() : left;
        }
        else if (std::abs(rect.right() - gridRect.right()) < threshold) {
            right = std::abs(rect.right()) < std::abs(right) ? rect.right() : right;
        }
    }
    if (left < std::numeric_limits<int>::max()) {
        gridRect.moveLeft(left);
    }
    if (right < std::numeric_limits<int>::max()) {
        gridRect.moveRight(right);
    }
    if (top < std::numeric_limits<int>::max()) {
        gridRect.moveTop(top);
    }
    if (bottom < std::numeric_limits<int>::max()) {
        gridRect.moveBottom(bottom);
    }
    return gridRect.topLeft().toPoint();
}