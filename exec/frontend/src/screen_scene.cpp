#include "screen_scene.hpp"

#include <QPainter>
#include <QGraphicsRectItem>
#include <QMimeData>
#include <QGraphicsSceneDragDropEvent>
#include <QGraphicsView>

#include "graphics_screen_item.hpp"
#include "materialtoast.hpp"

ScreenScene::ScreenScene(QObject *parent) : QGraphicsScene(parent) {}

ScreenScene::~ScreenScene() {}

void ScreenScene::drawBackground(QPainter *painter, const QRectF &rect)
{
    QGraphicsScene::drawBackground(painter, rect);
}

auto ScreenScene::get_screen_configs() const -> std::vector<mks::VirtualScreenConfig>
{
    std::vector<mks::VirtualScreenConfig> results;
    for (auto *item : items()) {
        if (auto *screenItem = dynamic_cast<GraphicsScreenItem *>(item); item != nullptr) {
            auto geometry = screenItem->get_item_geometry();
            auto name     = screenItem->get_screen_name();
            results.emplace_back(mks::VirtualScreenConfig{.name   = name.toStdString(),
                                                          .posX   = geometry.x(),
                                                          .posY   = geometry.y(),
                                                          .width  = geometry.width(),
                                                          .height = geometry.height()});
        }
    }
    return results;
}

auto ScreenScene::config_screens(const std::string                           &self,
                                 const std::vector<mks::VirtualScreenConfig> &configs) -> void
{
    for (const auto &config : configs) {
        auto *item = new GraphicsScreenItem(QString::fromStdString(config.name), 0,
                                            {config.width, config.height});
        addItem(item);
        if (self == config.name) {
            _selfItem = item;
        }
        item->setFlag(QGraphicsItem::ItemIsMovable, self != config.name);
        item->setPos(config.posX, config.posY);
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
        auto                screenId     = event->mimeData()->data("screenId").toInt();
        auto                screenWidth  = event->mimeData()->data("screenWidth").toInt();
        auto                screenHeight = event->mimeData()->data("screenHeight").toInt();
        GraphicsScreenItem *item         = new GraphicsScreenItem(
            text, screenId, {screenWidth, screenHeight}); // 创建一个新的文本项
        addItem(item);
        item->setFlag(QGraphicsItem::ItemIsMovable, true);
        item->update_grid_pos(event->scenePos() - QPoint{screenWidth, screenHeight} / 2);
        item->adsorption_to_grid(); // 网格吸附
        item->fit_font(views().back()->transform());
        event->setProposedAction(Qt::DropAction::MoveAction); // 设置放置动作
        event->acceptProposedAction();                        // 接受放置事件
    }
}

void ScreenScene::dragLeaveEvent([[maybe_unused]] QGraphicsSceneDragDropEvent *event) {}
