#include "materialtoast.hpp"

#include <QLabel>
#include <QVBoxLayout>
#include <QApplication>
#include <QScreen>
#include <QPainter>
#include <QPainterPath>
#include <QGraphicsDropShadowEffect>
#include <QDebug>
#include <algorithm>

// --- Static Show Method ---
void MaterialToast::show(QWidget *parent, const QString &message, int durationMs)
{
    // Attempt to find a suitable parent if none provided
    QWidget *effectiveParent = parent;
    if (effectiveParent == nullptr) {
        effectiveParent = QApplication::activeWindow();
        // Fallback if no active window (e.g., during startup)
        if ((effectiveParent == nullptr) && !QApplication::topLevelWidgets().isEmpty()) {
            effectiveParent = QApplication::topLevelWidgets().first();
        }
    }

    // Create the toast instance (it will manage its own lifetime)
    // Pass effectiveParent for positioning, but set parent to nullptr initially
    // so it becomes a top-level window.
    MaterialToast *toast =
        new MaterialToast(nullptr, message, durationMs); // Parent is nullptr for top-level

    // Calculate position based on the effective parent
    if (effectiveParent != nullptr) {
        QPoint targetPos    = effectiveParent->mapToGlobal(QPoint(0, 0)); // Top-left of parent
        int    parentWidth  = effectiveParent->width();
        int    toastWidth   = toast->width();
        int    toastHeight  = toast->height();

        // Position: Bottom-center, with some margin
        int posx = targetPos.x() + ((parentWidth - toastWidth) / 2);
        int posy = targetPos.y() + 20; // 20px margin from top

        // Ensure it stays within screen bounds (basic check)
        QRect screenGeometry = effectiveParent->screen()->availableGeometry();
        posx                 = std::max(posx, screenGeometry.x());
        posy                 = std::max(posy, screenGeometry.y());
        if (posx + toastWidth > screenGeometry.x() + screenGeometry.width()) {
            posx = screenGeometry.x() + screenGeometry.width() - toastWidth;
        }
        if (posy + toastHeight > screenGeometry.y() + screenGeometry.height()) {
            posy =
                screenGeometry.y() + screenGeometry.height() - toastHeight - 5; // Adjust if too low
        }

        toast->move(posx, posy);
    }
    else {
        // Fallback positioning if no parent found (center of primary screen)
        QRect screenGeometry = QApplication::primaryScreen()->availableGeometry();
        toast->move((screenGeometry.width() - toast->width()) / 2,
                    (screenGeometry.height() - toast->height()) / 2);
        qWarning() << "MaterialToast: Could not determine parent widget, centering on screen.";
    }

    // Start the showing process
    toast->_start();
}

// --- Private Constructor ---
MaterialToast::MaterialToast(QWidget *parent, const QString &message, int durationMs)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::ToolTip |
                          Qt::WindowStaysOnTopHint), // Flags for pop-up behavior
      _hideTimer(new QTimer(this)), _animation(new QPropertyAnimation(this, "windowOpacity", this)),
      /*_shadowEffect(new QGraphicsDropShadowEffect(this)),*/ _durationMs(durationMs),
      _cornerRadius(6.0),                        // Adjust corner radius as needed
      _backgroundColor(QColor(50, 50, 50, 230)), // Dark semi-transparent background
      _textColor(Qt::white)
{
    // Basic setup
    setAttribute(Qt::WA_TranslucentBackground); // Needed for rounded corners and transparency
    setAttribute(Qt::WA_DeleteOnClose);         // Ensure deletion

    // Label setup
    _messageLabel = new QLabel(message, this);
    _messageLabel->setAlignment(Qt::AlignCenter);
    _messageLabel->setWordWrap(true); // Allow text wrapping

    // Palette for colors
    QPalette pal = _messageLabel->palette();
    pal.setColor(QPalette::WindowText, _textColor); // Set text color
    _messageLabel->setPalette(pal);
    _messageLabel->setAutoFillBackground(false); // Label doesn't fill background

    // Layout
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(_messageLabel);
    layout->setContentsMargins(15, 10, 15, 10); // Padding inside the toast
    setLayout(layout);

    // Adjust size based on content (do this *after* setting text and margins)
    adjustSize();
    // Optional: Set a min/max width if needed
    setMinimumWidth(150);
    setMaximumWidth(400);
    adjustSize(); // Adjust again after width constraints

    // Shadow Effect (Material style)
    // _shadowEffect->setBlurRadius(15);
    // _shadowEffect->setColor(QColor(0, 0, 0, 80)); // Subtle black shadow
    // _shadowEffect->setOffset(0, 2);               // Small vertical offset
    // setGraphicsEffect(_shadowEffect);

    // Timer setup
    _hideTimer->setSingleShot(true);
    connect(_hideTimer, &QTimer::timeout, this, &MaterialToast::_hide_toast);

    // Animation setup (Fade)
    _animation->setDuration(300); // Fade duration
    connect(_animation, &QPropertyAnimation::finished, this,
            &MaterialToast::_on_hide_animation_finished); // Connect finished for deletion
}

MaterialToast::~MaterialToast()
{
    // No need to delete children explicitly due to Qt's parent-child ownership,
    // but good practice if effect wasn't parented (it is here).
    // qDebug() << "MaterialToast destroyed";
}

void MaterialToast::_set_message(const QString &message)
{
    _messageLabel->setText(message);
    adjustSize(); // Adjust size if text changes significantly
}

void MaterialToast::_set_duration(int durationMs)
{
    _durationMs = durationMs;
}

void MaterialToast::_start()
{
    setWindowOpacity(0.0); // Start fully transparent
    QWidget::show();       // Make widget visible (but transparent)

    _animation->setStartValue(0.0);
    _animation->setEndValue(1.0); // Fade in
    _animation->start();

    _hideTimer->start(_durationMs); // Start timer to hide later
}

void MaterialToast::_hide_toast()
{
    // Don't restart timer if already hiding
    if (_animation->state() == QAbstractAnimation::Running && _animation->endValue() == 0.0) {
        return;
    }

    _hideTimer->stop(); // Stop timer in case called manually before timeout

    _animation->setStartValue(windowOpacity()); // Start fade from current opacity
    _animation->setEndValue(0.0);               // Fade out
    _animation->start();
}

// Slot called *after* the hide animation finishes
void MaterialToast::_on_hide_animation_finished()
{
    // Only delete if the animation was fading out
    if (_animation->endValue() == 0.0) {
        close(); // Close the widget
        // deleteLater(); // WA_DeleteOnClose should handle this, but explicit deleteLater is safer
    }
}

// --- Event Overrides ---

void MaterialToast::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Path for rounded rectangle
    QPainterPath path;
    path.addRoundedRect(rect(), _cornerRadius, _cornerRadius);

    // Clip painting to the rounded rectangle
    painter.setClipPath(path);

    // Fill background
    painter.fillPath(path, _backgroundColor);

    // Note: Text is drawn by the QLabel child widget
}

void MaterialToast::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // Optional: Could recalculate label size constraints here if needed
}