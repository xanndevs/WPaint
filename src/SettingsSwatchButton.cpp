#include "SettingsSwatchButton.h"
#include "Theme.h"

#include <QMouseEvent>
#include <QPainter>

ColorSwatchButton::ColorSwatchButton(const QColor& color, const QString& title,
                                     QWidget* parent)
    : QWidget(parent), m_color(color) {
    setObjectName("SettingsSwatch");
    setFixedSize(48, 28);
    setCursor(Qt::PointingHandCursor);
    setToolTip(title);
}

void ColorSwatchButton::setColor(const QColor& c) {
    if (!c.isValid() || c == m_color) return;
    m_color = c;
    update();
}

void ColorSwatchButton::paintEvent(QPaintEvent* ev) {
    Q_UNUSED(ev);
    const auto& t = Theme::tokens();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(t.controlStroke, 1));
    p.setBrush(m_color);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), t.radiusSm,
                      t.radiusSm);
}

void ColorSwatchButton::mouseReleaseEvent(QMouseEvent* ev) {
    if (ev->button() == Qt::LeftButton)
        emit picked();
}
