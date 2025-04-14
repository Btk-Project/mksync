#include "screen_edit_view.hpp"

#include <QScrollBar>
#include <QDragMoveEvent>
#include <QHBoxLayout>
#include <QMenu>
#include <QAction>

#include "screen_scene.hpp"

const QString g_buttonStyleTemplate = R"(QPushButton{
    border:1px solid %1;
    background-color: %1;
    border-radius: %2px;
    width: %3px;
    height: %3px;
    border-image: url(%4);
}
QPushButton:hover {
    background-color: %5;
}
QPushButton:pressed
{
    background-color: %6;
}
)";

ScreenEditView::ScreenEditView(QWidget *parent) : QGraphicsView(parent)
{
    _suspensionWidget = new QWidget(this);
    auto *layout      = new QHBoxLayout(_suspensionWidget);
    _suspensionWidget->setLayout(layout);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    _dragPoint = new QLabel(this);
    _dragPoint->setFixedSize(24, 24);
    _refreshButton = new QPushButton(this);
    _refreshButton->setFixedSize(24, 24);
    _locationButton = new QPushButton(this);
    _locationButton->setFixedSize(24, 24);
    _pushpinButton = new QCheckBox(this);
    _pushpinButton->setFixedSize(24, 24);
    _fitViewButton = new QPushButton(this);
    _fitViewButton->setFixedSize(24, 24);
    _zoomInButton = new QPushButton(this);
    _zoomInButton->setFixedSize(24, 24);
    _zoomOutButton = new QPushButton(this);
    _zoomOutButton->setFixedSize(24, 24);

    _suspensionWidget->setStyleSheet("QWidget{background-color: transparent;}");

    layout->addWidget(_dragPoint);
    layout->addWidget(_pushpinButton);
    layout->addWidget(_refreshButton);
    layout->addWidget(_locationButton);
    layout->addWidget(_fitViewButton);
    layout->addWidget(_zoomInButton);
    layout->addWidget(_zoomOutButton);

    _dragPoint->installEventFilter(this);
    _dragPoint->setStyleSheet(R"(
QLabel{
    border:1px solid #30FFFFFF;
    background-color: #30FFFFFF;
    border-radius: 5px;
    width: 24px;
    height: 24px;
    border-image: url(:/icons/widget/v_drag_point.png);
})");
    _refreshButton->setStyleSheet(g_buttonStyleTemplate.arg("#30FFFFFF")
                                      .arg("5")
                                      .arg(24)
                                      .arg(":/icons/widget/refresh.png")
                                      .arg("#A3EADDFF")
                                      .arg("#EADDFF"));
    _locationButton->setStyleSheet(g_buttonStyleTemplate.arg("#30FFFFFF")
                                       .arg("5")
                                       .arg(24)
                                       .arg(":/icons/widget/location.png")
                                       .arg("#A3EADDFF")
                                       .arg("#EADDFF"));
    _fitViewButton->setStyleSheet(g_buttonStyleTemplate.arg("#30FFFFFF")
                                      .arg("5")
                                      .arg(24)
                                      .arg(":/icons/widget/fit_view.png")
                                      .arg("#A3EADDFF")
                                      .arg("#EADDFF"));
    _zoomInButton->setStyleSheet(g_buttonStyleTemplate.arg("#30FFFFFF")
                                     .arg("5")
                                     .arg(24)
                                     .arg(":/icons/widget/zoom_in.png")
                                     .arg("#A3EADDFF")
                                     .arg("#EADDFF"));
    _zoomOutButton->setStyleSheet(g_buttonStyleTemplate.arg("#30FFFFFF")
                                      .arg("5")
                                      .arg(24)
                                      .arg(":/icons/widget/zoom_out.png")
                                      .arg("#A3EADDFF")
                                      .arg("#EADDFF"));
    _pushpinButton->setStyleSheet(
        R"(
QCheckBox::indicator {
    width: 24px;
    height: 24px;
}
QCheckBox::indicator:unchecked {
    border: 1px solid #30FFFFFF;
    background-color: #30FFFFFF;
    border-radius: 5px;
    width: 24px;
    height: 24px;
    border-image: url(:/icons/widget/pushpin.png);
}
QCheckBox::indicator:checked {
    border: 1px solid #307D5260;
    background-color: #307D5260;
    border-radius: 5px;
    width: 24px;
    height: 24px;
    border-image: url(:/icons/widget/pushpin.png);
}
QCheckBox::indicator:checked:hover {
    background-color: #A3633B48;
}
QCheckBox::indicator:checked:pressed {
    background-color: #633B48;
}
QCheckBox::indicator:unchecked:hover {
    background-color: #A3EADDFF;
}
QCheckBox::indicator:unchecked:pressed{
    background-color: #EADDFF;
})");
    connect(_locationButton, &QPushButton::clicked, this, &ScreenEditView::location_button_clicked);
    connect(_pushpinButton, &QCheckBox::clicked, this, &ScreenEditView::pushpin_button_clicked);
    connect(_fitViewButton, &QPushButton::clicked, this, &ScreenEditView::fit_view_to_scene);
    connect(_zoomInButton, &QPushButton::clicked, this, [this]() { zoom_in_out_view(1.1); });
    connect(_zoomOutButton, &QPushButton::clicked, this, [this]() { zoom_in_out_view(0.9); });
    connect(_refreshButton, &QPushButton::clicked, this, &ScreenEditView::refresh_screens_request);
}

void ScreenEditView::location_button_clicked()
{
    auto *screenScene = dynamic_cast<ScreenScene *>(scene());
    if (screenScene != nullptr) {
        if (screenScene->get_self_item() != nullptr) {
            centerOn(screenScene->get_self_item());
        }
        else {
            centerOn(0, 0);
        }
    }
}

void ScreenEditView::pushpin_button_clicked(bool checked)
{
    _isPushpin = checked;
    horizontalScrollBar()->setDisabled(checked);
    horizontalScrollBar()->setVisible(!checked);
    verticalScrollBar()->setDisabled(checked);
    verticalScrollBar()->setVisible(!checked);
}

void ScreenEditView::fit_view_to_scene()
{
    // 获取场景的边界矩形
    QRectF sceneRect    = scene()->itemsBoundingRect();
    QRectF adjustedRect = sceneRect.adjusted(-20, -20, 20, 20); // 添加 20px 边距
    if (!sceneRect.isEmpty()) {
        // 调整视图以适配场景内容
        fitInView(adjustedRect, Qt::KeepAspectRatio);
        if (transform().m11() > 0.05) { // 如果缩放比例大于0.05, 则缩小到0.05
            setTransform(transform() *
                         QTransform::fromScale(0.05 / transform().m11(), 0.05 / transform().m11()));
        }
        else if (transform().m11() < 0.0001) { // 如果缩放比例小于0.0001, 则放大到0.0001
            setTransform(transform() * QTransform::fromScale(0.0001 / transform().m11(),
                                                             0.0001 / transform().m11()));
        }
    }
    else {
        qWarning() << "Scene is empty, cannot fit view.";
    }
    auto *mscene = dynamic_cast<ScreenScene *>(scene());
    if (mscene != nullptr) {
        for (auto *item : mscene->items()) {
            if (auto *sitem = dynamic_cast<GraphicsScreenItem *>(item); sitem != nullptr) {
                sitem->fit_font(transform());
            }
        }
    }
}

void ScreenEditView::zoom_in_out_view(float scale)
{
    setTransform(transform() * QTransform::fromScale(scale, scale));
}

void ScreenEditView::mousePressEvent(QMouseEvent *event)
{
    if (auto *item = itemAt(event->pos());
        item != nullptr && ((item->flags() & QGraphicsItem::ItemIsMovable) != 0)) {
        _isItemMoving = true;
    }
    return QGraphicsView::mousePressEvent(event);
}

void ScreenEditView::mouseMoveEvent(QMouseEvent *event)
{
    QGraphicsView::mouseMoveEvent(event);
    if (!_isPushpin && _isItemMoving) {
        _horizontalValue = horizontalScrollBar()->value();
        _verticalValue   = verticalScrollBar()->value();
        _update_view_area(event->position());
    }
}

void ScreenEditView::mouseReleaseEvent(QMouseEvent *event)
{
    _isItemMoving = false;
    return QGraphicsView::mouseReleaseEvent(event);
}

void ScreenEditView::wheelEvent(QWheelEvent *event)
{
    if ((event->modifiers() & Qt::ControlModifier) != 0) {
        setResizeAnchor(QGraphicsView::AnchorUnderMouse);
        if (event->angleDelta().y() > 0) {
            zoom_in_out_view(1.1);
        }
        else {
            zoom_in_out_view(0.9);
        }
        setResizeAnchor(QGraphicsView::AnchorViewCenter);
    }
    else {
        QGraphicsView::wheelEvent(event);
    }
}

void ScreenEditView::dragEnterEvent(QDragEnterEvent *event)
{
    return QGraphicsView::dragEnterEvent(event);
}

void ScreenEditView::dragLeaveEvent(QDragLeaveEvent *event)
{
    return QGraphicsView::dragLeaveEvent(event);
}

void ScreenEditView::dragMoveEvent(QDragMoveEvent *event)
{
    _horizontalValue = horizontalScrollBar()->value();
    _verticalValue   = verticalScrollBar()->value();
    QGraphicsView::dragMoveEvent(event);
    if (!_isPushpin) {
        _update_view_area(event->position());
    }
}

void ScreenEditView::dropEvent(QDropEvent *event)
{
    return QGraphicsView::dropEvent(event);
}

bool ScreenEditView::eventFilter(QObject *object, QEvent *event)
{
    if (object == _dragPoint && event->type() == QEvent::MouseButtonPress) {
        _isToolMenuMoving = true;
        return true;
    }
    if (object == _dragPoint && event->type() == QEvent::MouseButtonRelease) {
        _isToolMenuMoving = false;
        return true;
    }
    if (object == _dragPoint && event->type() == QEvent::MouseMove) {
        if (_isToolMenuMoving) {
            auto *evnt = dynamic_cast<QMouseEvent *>(event);
            _suspensionWidget->move(_suspensionWidget->mapTo(this, evnt->pos()) -
                                    QPoint(_dragPoint->width(), _dragPoint->height()) / 2);
        }
        return true;
    }
    return QGraphicsView::eventFilter(object, event);
}

void ScreenEditView::contextMenuEvent(QContextMenuEvent *event)
{
    QPoint  viewPos  = event->pos();
    QPointF scenePos = mapToScene(viewPos);

    // 尝试获取点击位置的 Item
    QGraphicsItem *itemUnderMouse = itemAt(viewPos);
    auto *textItem = dynamic_cast<GraphicsScreenItem *>(itemUnderMouse); // 尝试转换为你的自定义类型

    QMenu menu(this);
    menu.setStyleSheet(R"(QMenu {
    background-color: white;
    margin: 2px; /* some spacing around the menu */
}

QMenu::item {
    color: black;
    padding: 2px 25px 2px 20px;
    border: 1px solid transparent; /* reserve space for selection border */
}

QMenu::item:selected {
    border-color: darkblue;
    background: rgba(100, 100, 100, 150);
}

QMenu::icon:checked { /* appearance of a 'checked' icon */
    background: gray;
    border: 1px inset gray;
    position: absolute;
    top: 1px;
    right: 1px;
    bottom: 1px;
    left: 1px;
}

QMenu::separator {
    height: 2px;
    background: lightblue;
    margin-left: 10px;
    margin-right: 5px;
}

QMenu::indicator {
    width: 13px;
    height: 13px;
})");
    if (textItem != nullptr) {
        // --- 点击在 Screen 上 ---
        qDebug() << "Right click on item:" << textItem->toPlainText();

        QAction *renameAction = menu.addAction(tr("rename"));
        if ((textItem->flags() & QGraphicsItem::ItemIsMovable) != 0) {
            QAction *deleteAction = menu.addAction(tr("delete"));
            connect(deleteAction, &QAction::triggered, this,
                    [this, textItem]() { delete_item(textItem); });
        }

        // 使用 Lambda 捕获 item 指针
        connect(renameAction, &QAction::triggered, this,
                [this, textItem]() { rename_item(textItem); });
    }
    else {
        // --- 点击在空白区域 ---
        qDebug() << "Right click on empty space at scene pos:" << scenePos;

        QAction *createAction = menu.addAction("new Item");

        // 使用 Lambda 捕获 scenePos
        connect(createAction, &QAction::triggered, this,
                [this, scenePos]() { create_item(scenePos.toPoint()); });
    }

    // 在鼠标光标的全局位置显示菜单
    menu.exec(event->globalPos());
    // return QGraphicsView::contextMenuEvent(event);
}

void ScreenEditView::rename_item(GraphicsScreenItem *item)
{
    if (item == nullptr) {
        return;
    }
    item->edit();
}

void ScreenEditView::delete_item(GraphicsScreenItem *item)
{
    if (item != nullptr) {
        auto *ss = dynamic_cast<ScreenScene *>(scene());
        if (ss != nullptr) {
            Q_EMIT ss->remove_screen(item->get_screen_name(), item->get_item_geometry().size());
        }
        scene()->removeItem(item);
        delete item;
    }
}

void ScreenEditView::create_item(QPoint scenePos)
{
    auto *ss = dynamic_cast<ScreenScene *>(scene());
    if (ss != nullptr) {
        if (auto *item = ss->new_screen_item("newScreen", scenePos, QSize(1920, 1080));
            item != nullptr) {
            rename_item(item);
        }
    }
    else {
        qDebug() << "Scene is not ScreenScene";
    }
}

void ScreenEditView::_update_view_area(QPointF pos)
{
    if (pos.x() < 10 || pos.x() > width() - 50) {
        _horizontalValue += (pos.x() < 10 ? -1 : 1);
        horizontalScrollBar()->setValue(_horizontalValue);
    }
    if (pos.y() < 10 || pos.y() > height() - 50) {
        _verticalValue += (pos.y() < 10 ? -1 : 1);
        verticalScrollBar()->setValue(_verticalValue);
    }
}
