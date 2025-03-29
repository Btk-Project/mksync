#pragma once

#include <QWidget>
#include <QString>
#include <QTimer>
#include <QPropertyAnimation> // For animations

class QLabel;
class QGraphicsDropShadowEffect;

class MaterialToast : public QWidget {
    Q_OBJECT
    // Property for fade animation
    Q_PROPERTY(qreal windowOpacity READ windowOpacity WRITE setWindowOpacity)

public:
    // Static method for easy global showing
    static void show(QWidget *parent, const QString &message, int durationMs = 3000);

    // Explicit constructor is private: use the static show method
private:
    explicit MaterialToast(QWidget *parent, const QString &message, int durationMs);
    ~MaterialToast() override; // Ensure effect is cleaned up

    void _set_message(const QString &message);
    void _set_duration(int durationMs);

    // Starts the show animation and timer
    void _start();

protected:
    // For custom drawing (rounded corners)
    void paintEvent(QPaintEvent *event) override;
    // Adjust layout on resize (optional but good practice)
    void resizeEvent(QResizeEvent *event) override;

private slots:
    // Starts the hide animation when timer times out
    void _hide_toast();
    // Deletes the toast after hide animation finishes
    void _on_hide_animation_finished();

private:
    QLabel                    *_messageLabel;
    QTimer                    *_hideTimer;
    QPropertyAnimation        *_animation;
    QGraphicsDropShadowEffect *_shadowEffect; // For the shadow

    int    _durationMs;
    qreal  _cornerRadius; // Radius for rounded corners
    QColor _backgroundColor;
    QColor _textColor;
};
