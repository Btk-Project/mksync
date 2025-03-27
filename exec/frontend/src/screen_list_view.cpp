#include "screen_list_view.hpp"

#include <QListWidgetItem>
#include <QMimeData>
#include <QDrag>
#include <QPainter>
#include <QPixmap>

ScreenListView::ScreenListView(QWidget *parent) : QListWidget(parent) {}

QListWidgetItem *ScreenListView::make_screen_item(const QString &name, int screenId, int width,
                                                  int height)
{
    return make_screen_item(name, screenId, {width, height});
}

QListWidgetItem *ScreenListView::make_screen_item(const QString &name, int screenId,
                                                  QSize screenSize)
{
    auto *item = new QListWidgetItem(name);
    item->setData(ScreenListViewRole::eScreenIdRole, screenId);
    item->setData(ScreenListViewRole::eScreenWidthRole, screenSize.width());
    item->setData(ScreenListViewRole::eScreenHeightRole, screenSize.height());

    return item;
}

void ScreenListView::startDrag(Qt::DropActions supportedActions)
{
    QListWidgetItem *item = currentItem();
    if (item == nullptr) {
        return;
    }

    QMimeData *mimeData = new QMimeData;
    mimeData->setText(item->text()); // 将文本作为拖动数据
    auto screenId = item->data(ScreenListViewRole::eScreenIdRole).toInt();
    auto width    = item->data(ScreenListViewRole::eScreenWidthRole).toInt();
    auto height   = item->data(ScreenListViewRole::eScreenHeightRole).toInt();
    mimeData->setData("screenId", QString::number(screenId).toLocal8Bit());
    mimeData->setData("screenWidth", QString::number(width).toLocal8Bit());
    mimeData->setData("screenHeight", QString::number(height).toLocal8Bit());

    QDrag *drag = new QDrag(this);
    drag->setMimeData(mimeData);
    QPixmap  pixmap(width * 0.05, height * 0.05);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    // 绘制背景的矩形框
    painter.setBrush(QColor("#6750A4"));
    painter.setPen(Qt::black);
    painter.drawRect(0, 0, width * 0.05, height * 0.05);
    // 绘制屏幕名称
    auto font = painter.font();
    font.setPixelSize(std::min(width * 0.05, height * 0.05) / 4);
    painter.setFont(font);
    painter.setPen(Qt::black);
    painter.drawText(0, 0, width * 0.05, height * 0.05, Qt::AlignCenter, item->text());
    painter.end();
    drag->setPixmap(pixmap);
    drag->setHotSpot(QPoint(width * 0.05 / 2, height * 0.05 / 2)); // 设置拖拽热点（图标中心）
    // 如果拖动操作成功，则删除拖动项
    if (drag->exec(supportedActions) == Qt::MoveAction) {
        delete takeItem(row(item));
    }
}