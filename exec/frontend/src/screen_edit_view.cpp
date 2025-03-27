#include "screen_edit_view.hpp"

#include <QScrollBar>
#include <QDragMoveEvent>
#include <QHBoxLayout>

#include "screen_scene.hpp"

ScreenEditView::ScreenEditView(QWidget *parent) : QGraphicsView(parent)
{
    _suspensionWidget = new QWidget(this);
    auto *layout      = new QHBoxLayout(_suspensionWidget);
    _suspensionWidget->setLayout(layout);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    _dragPoint     = new QLabel(this);
    _refreshButton = new QPushButton(this);
    _pushpinButton = new QCheckBox(this);
    _fitViewButton = new QPushButton(this);
    _zoomInButton  = new QPushButton(this);
    _zoomOutButton = new QPushButton(this);

    _suspensionWidget->setStyleSheet("QWidget{background-color: transparent;}");

    layout->addWidget(_dragPoint);
    layout->addWidget(_pushpinButton);
    layout->addWidget(_refreshButton);
    layout->addWidget(_fitViewButton);
    layout->addWidget(_zoomInButton);
    layout->addWidget(_zoomOutButton);

    _dragPoint->installEventFilter(this);
    _dragPoint->setFixedSize(18, 18);
    _dragPoint->setStyleSheet(R"(
QLabel{
    border:1px solid #30FFFFFF;
    background-color: #30FFFFFF;
    border-radius: 5px;
    width: 18px;
    height: 18px;
    border-image: url(:/icons/widget/v_drag_point.png);
})");
    _refreshButton->setStyleSheet(
        R"(QPushButton{
    border:1px solid #30FFFFFF;
    background-color: #30FFFFFF;
    border-radius: 5px;
    width: 18px;
    height: 18px;
    border-image: url(:/icons/widget/location.png);
}
QPushButton:hover {
    background-color: #A3EADDFF;
}
QPushButton:pressed
{
    background-color: #EADDFF;
}
)");

    _fitViewButton->setStyleSheet(
        R"(QPushButton{
    border:1px solid #30FFFFFF;
    background-color: #30FFFFFF;
    border-radius: 5px;
    width: 18px;
    height: 18px;
    border-image: url(:/icons/widget/fit_view.png);
}
QPushButton:hover {
    background-color: #A3EADDFF;
}
QPushButton:pressed
{
    background-color: #EADDFF;
}
)");

    _zoomInButton->setStyleSheet(
        R"(QPushButton{
    border:1px solid #30FFFFFF;
    background-color: #30FFFFFF;
    border-radius: 5px;
    width: 18px;
    height: 18px;
    border-image: url(:/icons/widget/zoom_in.png);
}
QPushButton:hover {
    background-color: #A3EADDFF;
}
QPushButton:pressed
{
    background-color: #EADDFF;
}
)");

    _zoomOutButton->setStyleSheet(
        R"(QPushButton{
    border:1px solid #30FFFFFF;
    background-color: #30FFFFFF;
    border-radius: 5px;
    width: 18px;
    height: 18px;
    border-image: url(:/icons/widget/zoom_out.png);
}
QPushButton:hover {
    background-color: #A3EADDFF;
}
QPushButton:pressed
{
    background-color: #EADDFF;
}
)");

    _pushpinButton->setStyleSheet(
        R"(
QCheckBox::indicator {
    width: 18px;
    height: 18px;
}
QCheckBox::indicator:unchecked {
    border: 1px solid #30FFFFFF;
    background-color: #30FFFFFF;
    border-radius: 5px;
    width: 18px;
    height: 18px;
    border-image: url(:/icons/widget/pushpin.png);
}
QCheckBox::indicator:checked {
    border: 1px solid #307D5260;
    background-color: #307D5260;
    border-radius: 5px;
    width: 18px;
    height: 18px;
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
    connect(_refreshButton, &QPushButton::clicked, this, &ScreenEditView::refresh_button_clicked);
    connect(_pushpinButton, &QCheckBox::clicked, this, &ScreenEditView::pushpin_button_clicked);
    connect(_fitViewButton, &QPushButton::clicked, this, &ScreenEditView::fit_view_to_scene);
    connect(_zoomInButton, &QPushButton::clicked, this, [this]() { zoom_in_out_view(1.1); });
    connect(_zoomOutButton, &QPushButton::clicked, this, [this]() { zoom_in_out_view(0.9); });
}

void ScreenEditView::refresh_button_clicked()
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
}

void ScreenEditView::zoom_in_out_view(float scale)
{
    setTransform(transform() * QTransform::fromScale(scale, scale));
}

void ScreenEditView::mousePressEvent(QMouseEvent *event)
{
    return QGraphicsView::mousePressEvent(event);
}

void ScreenEditView::mouseMoveEvent(QMouseEvent *event)
{
    _horizontalValue = horizontalScrollBar()->value();
    _verticalValue   = verticalScrollBar()->value();
    QGraphicsView::mouseMoveEvent(event);
    if (!_isPushpin) {
        _update_view_area(event->position());
    }
}

void ScreenEditView::mouseReleaseEvent(QMouseEvent *event)
{
    return QGraphicsView::mouseReleaseEvent(event);
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
