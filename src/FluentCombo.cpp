#include "FluentCombo.h"
#include "Theme.h"

#include <QPainter>
#include <QPaintEvent>

FluentCombo::FluentCombo(QWidget* parent) : QComboBox(parent) {
    setObjectName("FluentCombo");
}

void FluentCombo::paintEvent(QPaintEvent* ev) {
    QComboBox::paintEvent(ev);

    const auto& t = Theme::tokens();
    const int glyph = t.caretGlyph;
    const int dropW = 22;
    const QRectF drop(width() - dropW, 0, dropW, height());
    // Drawn rather than styled for the same reason as the switch: the glyph has
    // to be the tinted icon, and a QSS image is baked at its marker colour.
    const qreal dpr = qMax<qreal>(2.0, devicePixelRatioF());
    const QPixmap pm = Theme::icon("chevron-down", glyph, t.icon)
                           .pixmap(QSize(glyph, glyph), dpr);
    if (pm.isNull()) return;
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawPixmap(drop.center().toPoint() - QPoint(glyph / 2, glyph / 2), pm);
}
