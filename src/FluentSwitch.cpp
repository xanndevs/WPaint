#include "FluentSwitch.h"
#include "Theme.h"

#include <QKeyEvent>
#include <QPainter>
#include <QPaintEvent>

FluentSwitch::FluentSwitch(QWidget* parent) : QAbstractButton(parent) {
    setCheckable(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover, true);
}

QSize FluentSwitch::sizeHint() const {
    const auto& t = Theme::tokens();
    // A little taller than the track: a 20px pill is a poor pointer target, and
    // the row it sits in is the comfortable size anyway.
    return QSize(t.switchW, t.switchH + 2 * t.gap);
}

void FluentSwitch::paintEvent(QPaintEvent* ev) {
    Q_UNUSED(ev);
    const auto& t = Theme::tokens();
    const bool on = isChecked();
    const int trackH = t.switchH;
    const int trackW = t.switchW;
    // Centred in the widget, which is taller than the track so the target is.
    const QRectF track((width() - trackW) / 2.0, (height() - trackH) / 2.0, trackW, trackH);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Track: accent when on, the plain control fill when off. A hover does not
    // change the track -- the thumb has already moved, and a second cue on top
    // of that only makes the state harder to read.
    p.setPen(QPen(t.controlStroke, 1));
    p.setBrush(on ? t.accent : t.surfaceHigh);
    p.drawRoundedRect(track.adjusted(0.5, 0.5, -0.5, -0.5), trackH / 2.0, trackH / 2.0);

    // Thumb: filled with whatever the track is not, so it reads as a hole in
    // the accent pill and as a solid dot on an empty one.
    const int inset = (trackH - t.switchThumb) / 2;
    const qreal x = on ? track.right() - inset - t.switchThumb : track.left() + inset;
    const QRectF thumb(x, track.top() + inset, t.switchThumb, t.switchThumb);
    p.setPen(Qt::NoPen);
    p.setBrush(on ? t.textOnAccent : t.textSecondary);
    p.drawEllipse(thumb);

    if (hasFocus()) {
        p.setPen(QPen(t.focusRing, 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(track.adjusted(-1.5, -1.5, 1.5, 1.5), trackH / 2.0 + 1.5,
                          trackH / 2.0 + 1.5);
    }
}

void FluentSwitch::keyPressEvent(QKeyEvent* ev) {
    // Space toggles a focused switch, matching every other Fluent control.
    if (ev->key() == Qt::Key_Space) {
        toggle();
        ev->accept();
        return;
    }
    QAbstractButton::keyPressEvent(ev);
}
