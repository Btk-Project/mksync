#include "graphics_screen_item.hpp"

#include <QPainter>
#include <QGraphicsScene>
#include <QList>
#include <QGraphicsSceneMouseEvent>
#include <QDebug>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QApplication>
#include <QGraphicsView>

#include "screen_scene.hpp"
#include "detail/rect_algorithm.hpp"

GraphicsScreenItem::GraphicsScreenItem(const QString &screenName, const int &screenId,
                                       const QSize &itemSize)
    : _screenName(screenName), _screenId(screenId), _itemSize(itemSize)
{
    _path.addRect(0, 0, _itemSize.width(), _itemSize.height());
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
    _itemSize = itemGeometry.size();
    setPos(itemGeometry.topLeft());
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
    return QRect(scenePos().toPoint(), _itemSize);
}

QPainterPath GraphicsScreenItem::shape() const
{
    QPainterPath path;
    path.addRect(boundingRect().adjusted(1, 1, -1, -1));
    return path;
}

QRectF GraphicsScreenItem::boundingRect() const
{
    return QRectF(0, 0, _itemSize.width(), _itemSize.height());
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
    painter->setPen(Qt::black);
    if (_isReachable) {
        painter->setBrush(Qt::green);
    }
    else {
        painter->setBrush(Qt::red);
    }
    painter->drawRect(rect);
    // 绘制背景的矩形框
    if (zValue() > 0) {
        painter->setBrush(QColor("#6750A4"));
    }
    else {
        painter->setBrush(QColor("#544597"));
    }
    auto rw = rect.width() / 100;
    auto rh = rect.height() / 100;
    painter->drawRect(rect.adjusted(rw, rh, -rw, -rh));
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
    if (textWidth > _itemSize.width()) {
        _showText = fm.elidedText(_screenName, Qt::ElideRight, _itemSize.width());
    }
    else {
        _showText = _screenName;
    }
}

void GraphicsScreenItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if ((flags() & QGraphicsItem::ItemIsMovable) != 0) {
        setZValue(1);
    }
    QGraphicsItem::mousePressEvent(event);
}

// 鼠标释放时处理网格占用和交换
void GraphicsScreenItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    setZValue(0);
    QGraphicsItem::mouseReleaseEvent(event);
    // 直接移动到目标位置
    if ((flags() & QGraphicsItem::ItemIsMovable) != 0) {
        update_grid_pos(pos());
        adsorption_to_grid();
    }
}

void GraphicsScreenItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mouseMoveEvent(event);
}

auto GraphicsScreenItem::update_reachable(bool isReachable) -> void
{
    _isReachable = isReachable;
}

void GraphicsScreenItem::update_grid_pos(const QPointF &scenePos)
{
    auto gridPos = adsorption_to_grid(scene(), this, scenePos, _itemSize);
    _gridPos     = gridPos;
}

void GraphicsScreenItem::adsorption_to_grid()
{
    setPos(_gridPos);

    auto *scene = qobject_cast<ScreenScene *>(this->scene());
    if (scene != nullptr) {
        if (scene->collidingItems(this).size() >= 1) {
            Q_EMIT scene->remove_screen(get_screen_name(), _itemSize);
            scene->removeItem(this);
            this->deleteLater();
        }
    }
}

QPoint GraphicsScreenItem::adsorption_to_grid(QGraphicsScene *scene, QGraphicsItem *self,
                                              const QPointF &scenePos, const QSize &itemSize)
{
    auto gridRect  = QRectF{scenePos, itemSize};
    auto transform = scene->views().empty() ? QTransform{} : scene->views().at(0)->transform();
    const qreal threshold = 10 / transform.m11();
    qDebug() << "adsorption : " << gridRect << "************************************** ";
    for (auto *item : scene->items()) {
        if (self == item) {
            continue;
        }
        auto rect = item->sceneBoundingRect();
        // 检测边侧距离
        // 如果当前的矩形靠近了目标矩形的边界，则自动对齐到目标边界。
        qDebug() << "process: " << rect << " ------------------------------------- ";
        QRectF retRect  = gridRect;
        auto   overlap  = detail::is_rect_overlap(gridRect, rect);
        int    touching = 0;
        if (((overlap & 2) != 0) && rect.left() - gridRect.right() < threshold &&
            rect.left() - gridRect.right() >= -(rect.width() + gridRect.width()) / 2) {
            retRect.moveRight(rect.left());
            qDebug() << "touching right" << rect << ": mvoe rectRect to" << retRect;
            touching = 1;
        }
        else if (((overlap & 2) != 0) && gridRect.left() - rect.right() < threshold &&
                 gridRect.left() - rect.right() >= -(rect.width() + gridRect.width()) / 2) {
            retRect.moveLeft(rect.right());
            qDebug() << "touching left" << rect << ": mvoe rectRect to" << retRect;
            touching = 1;
        }
        if (((overlap & 1) != 0) && rect.top() - gridRect.bottom() < threshold &&
            rect.top() - gridRect.bottom() >= -(rect.height() + gridRect.height()) / 2) {
            retRect.moveBottom(rect.top());
            qDebug() << "touching bottom" << rect << ": mvoe rectRect to" << retRect;
            touching = 2;
        }
        else if (((overlap & 1) != 0) && gridRect.top() - rect.bottom() < threshold &&
                 gridRect.top() - rect.bottom() >= -(rect.height() + gridRect.height()) / 2) {
            retRect.moveTop(rect.bottom());
            qDebug() << "touching top" << rect << ": mvoe rectRect to" << retRect;
            touching = 2;
        }
        if (touching == 0) {
            continue;
        }
        auto citems = scene->items(retRect);
        if ((citems.size() == 1 && citems.back() != self) || citems.size() > 1) {
            qDebug() << "overlap with other item " << citems.size();
            continue;
        }
        if (touching == 1) {
            if (std::abs(rect.top() - gridRect.top()) < threshold) {
                retRect.moveTop(rect.top());
                qDebug() << "Alignment top: " << retRect;
            }
            else if (std::abs(rect.bottom() - gridRect.bottom()) < threshold) {
                retRect.moveBottom(rect.bottom());
                qDebug() << "Alignment bottom: " << retRect;
            }
        }
        if (touching == 2) {
            if (std::abs(rect.left() - gridRect.left()) < threshold) {
                retRect.moveLeft(rect.left());
                qDebug() << "Alignment left: " << retRect;
            }
            else if (std::abs(rect.right() - gridRect.right()) < threshold) {
                retRect.moveRight(rect.right());
                qDebug() << "Alignment right: " << retRect;
            }
        }
        citems = scene->items(retRect);
        if ((citems.size() == 1 && citems.back() != self) || citems.size() > 1) {
            qDebug() << "overlap with other item " << citems.size();
            continue;
        }
        gridRect = retRect;
    }
    return gridRect.topLeft().toPoint();
}