#include "screen_scene.hpp"

#include <QPainter>
#include <QGraphicsRectItem>
#include <QMimeData>
#include <QGraphicsSceneDragDropEvent>
#include <QGraphicsView>
#include <QJsonObject>

#include "graphics_screen_item.hpp"
#include "materialtoast.hpp"

ScreenScene::ScreenScene(QObject *parent) : QGraphicsScene(parent) {}

ScreenScene::~ScreenScene() {}

void ScreenScene::drawBackground(QPainter *painter, const QRectF &rect)
{
    QGraphicsScene::drawBackground(painter, rect);
}

auto ScreenScene::get_screen_configs() const -> QJsonArray
{
    QJsonArray array;
    for (auto *item : items()) {
        if (auto *screenItem = dynamic_cast<GraphicsScreenItem *>(item); item != nullptr) {
            QJsonObject object;
            auto        geometry = screenItem->get_item_geometry();
            auto        name     = screenItem->get_screen_name();
            object["name"]       = name;
            object["posX"]       = geometry.x();
            object["posY"]       = geometry.y();
            object["width"]      = geometry.width();
            object["height"]     = geometry.height();
            array.append(object);
        }
    }
    return array;
}

auto ScreenScene::config_screens(const QString &self, const QJsonArray &configs) -> void
{
    for (const auto &config : configs) {
        assert(config.isObject());
        auto  cf   = config.toObject();
        auto  name = cf["name"].toString();
        auto *item = new GraphicsScreenItem(name, 0, {cf["width"].toInt(), cf["height"].toInt()});
        addItem(item);
        if (self == name) {
            _selfItem = item;
            _selfItem->update_online(true);
            _selfItem->update_reachable(true);
        }
        item->setFlag(QGraphicsItem::ItemIsMovable, self != name);
        item->setPos(cf["posX"].toInt(), cf["posY"].toInt());
    }
}

auto ScreenScene::get_self_item() -> GraphicsScreenItem *
{
    return _selfItem;
}

auto ScreenScene::contains_screen(const QString &screen) -> bool
{
    return std::ranges::any_of(items(), [screen](auto *item) {
        if (auto *screenItem = dynamic_cast<GraphicsScreenItem *>(item); item != nullptr) {
            return screenItem->get_screen_name() == screen;
        }
        return false;
    });
}

void ScreenScene::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
    if (event->mimeData()->hasText()) {
        event->acceptProposedAction(); // 接受拖动事件
    }
}

void ScreenScene::dragMoveEvent(QGraphicsSceneDragDropEvent *event)
{
    if (event->mimeData()->hasText()) {
        event->acceptProposedAction(); // 允许拖动
    }
}

void ScreenScene::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    QString text = event->mimeData()->text();
    if (contains_screen(text)) {
        event->setProposedAction(Qt::DropAction::IgnoreAction); // 忽略放置事件
        MaterialToast::show(nullptr, "屏幕已存在");
        event->acceptProposedAction();
    }
    else if (!text.isEmpty()) {
        auto screenId     = event->mimeData()->data("screenId").toInt();
        auto screenWidth  = event->mimeData()->data("screenWidth").toInt();
        auto screenHeight = event->mimeData()->data("screenHeight").toInt();
        if (new_screen_item(text, event->screenPos(), {screenWidth, screenHeight}, screenId) ==
            nullptr) {
            event->setProposedAction(Qt::DropAction::IgnoreAction); // 忽略放置事件
            event->acceptProposedAction();
        }
        else {
            event->setProposedAction(Qt::DropAction::MoveAction); // 设置放置动作
            event->acceptProposedAction();                        // 接受放置事件
        }
    }
}

auto ScreenScene::new_screen_item(QString screen, QPoint scenePos, QSize size, int id)
    -> GraphicsScreenItem *
{
    GraphicsScreenItem *item = new GraphicsScreenItem(screen, id, size); // 创建一个新的文本项
    addItem(item);
    item->setFont(font());
    item->setFlag(QGraphicsItem::ItemIsMovable, true);
    item->update_grid_pos(scenePos - QPoint{size.width(), size.height()} / 2);
    if (item->adsorption_to_grid()) {
        item->fit_font(views().back()->transform());
        Q_EMIT screen_moved(item->get_screen_name(), item->get_item_geometry().topLeft(), size);
        return item;
    }
    removeItem(item); // 如果无法对齐，则删除该项
    item->deleteLater();
    return nullptr;
}

bool ScreenScene::remove_screen_item(QString screen)
{
    for (auto *item : items()) {
        if (auto *screenItem = dynamic_cast<GraphicsScreenItem *>(item); item != nullptr) {
            if (screenItem->get_screen_name() == screen) {
                Q_EMIT remove_screen(screenItem->get_screen_name(),
                                     screenItem->get_item_geometry().size());
                removeItem(item);
                screenItem->deleteLater();
                return true;
            }
        }
    }
    return false;
}

void ScreenScene::dragLeaveEvent([[maybe_unused]] QGraphicsSceneDragDropEvent *event) {}
