#include "FluentSlider.h"

#include "Theme.h"

#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionSlider>

FluentSlider::FluentSlider(Qt::Orientation orientation, QWidget* parent)
    : QSlider(orientation, parent) {
    setFocusPolicy(Qt::StrongFocus);
}

QSize FluentSlider::sizeHint() const {
    QSize s = QSlider::sizeHint();
    const int halo = int(2.0 * Theme::tokens().sliderHandle * 0.68) + 4;
    if (orientation() == Qt::Vertical)
        s.setWidth(qMax(s.width(), halo));
    else
        s.setHeight(qMax(s.height(), halo));
    return s;
}

// Radius of the hover halo. Only the cross axis caps it, so it can never
// spill out of the thin dimension; along the track it is free to reach.
qreal FluentSlider::haloRadius() const {
    const int handle = Theme::tokens().sliderHandle;
    const int cross = (orientation() == Qt::Vertical) ? width() : height();
    return qMin(handle * 0.68, cross / 2.0 - 0.5);
}

// The track is inset along its length by the halo radius so the halo is still
// whole when the thumb sits at either end of its travel; a half-width inset
// left it overhanging the widget by a couple of pixels at the extremes. This
// uses the un-capped radius on purpose: deriving it from haloRadius() would
// make the geometry depend on the cross size, which the layout is still in the
// middle of deciding.
int FluentSlider::travelInset() const {
    return qCeil(Theme::tokens().sliderHandle * 0.68) + 1;
}

QRect FluentSlider::trackRect() const {
    const int groove = Theme::tokens().sliderGroove;
    const int inset = travelInset();
    if (orientation() == Qt::Vertical)
        return QRect((width() - groove) / 2, inset, groove, height() - 2 * inset);
    return QRect(inset, (height() - groove) / 2, width() - 2 * inset, groove);
}

QPointF FluentSlider::thumbCenter() const {
    QStyleOptionSlider opt;
    initStyleOption(&opt);
    const QRect t = trackRect();
    const bool vertical = orientation() == Qt::Vertical;
    const int travel = vertical ? t.height() : t.width();
    const int pos = QStyle::sliderPositionFromValue(
        opt.minimum, opt.maximum, opt.sliderPosition, travel, opt.upsideDown);
    return QPointF(vertical ? t.center().x() : t.left() + pos,
                   vertical ? t.top() + pos : t.center().y());
}

int FluentSlider::valueForPoint(const QPointF& p) const {
    QStyleOptionSlider opt;
    initStyleOption(&opt);
    const QRect t = trackRect();
    const bool vertical = orientation() == Qt::Vertical;
    const int pos = vertical ? int(p.y()) - t.top() : int(p.x()) - t.left();
    const int travel = vertical ? t.height() : t.width();
    const int clamped = qBound(0, pos, travel);
    return QStyle::sliderValueFromPosition(opt.minimum, opt.maximum, clamped,
                                           travel, opt.upsideDown);
}

void FluentSlider::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        const QPointF grab = event->position();
        const double d = QLineF(thumbCenter(), grab).length();
        if (d > Theme::tokens().sliderHandle / 2.0)
            setValue(valueForPoint(grab));
        update();
        event->accept();
        return;
    }
    QSlider::mousePressEvent(event);
}

void FluentSlider::mouseMoveEvent(QMouseEvent* event) {
    if (m_dragging) {
        setValue(valueForPoint(event->position()));
        update();
        event->accept();
        return;
    }
    QSlider::mouseMoveEvent(event);
}

void FluentSlider::mouseReleaseEvent(QMouseEvent* event) {
    if (m_dragging && event->button() == Qt::LeftButton) {
        m_dragging = false;
        setValue(valueForPoint(event->position()));
        update();
        event->accept();
        return;
    }
    QSlider::mouseReleaseEvent(event);
}

// QAbstractSlider's wheel handling steps the value but leaves the event
// unaccepted once the value is pinned at an end, so the size slider -- which
// floats over the canvas scroll area -- passed the gesture on and the canvas
// started scrolling vertically. The wheel is this control's, so it is always
// consumed; the size cannot move past its limits anyway, and passing the event
// along to scroll the document is never what the user meant by spinning over
// the brush size.
void FluentSlider::wheelEvent(QWheelEvent* event) {
    const int delta = event->angleDelta().y();
    if (delta != 0) {
        if (delta > 0)
            triggerAction(QAbstractSlider::SliderSingleStepAdd);
        else
            triggerAction(QAbstractSlider::SliderSingleStepSub);
    } else if (event->angleDelta().x() != 0 && orientation() == Qt::Horizontal) {
        if (event->angleDelta().x() > 0)
            triggerAction(QAbstractSlider::SliderSingleStepAdd);
        else
            triggerAction(QAbstractSlider::SliderSingleStepSub);
    }
    event->accept();
}

void FluentSlider::paintEvent(QPaintEvent*) {
    const auto& t = Theme::tokens();
    const bool vertical = orientation() == Qt::Vertical;
    const int groove = t.sliderGroove;
    const int handle = t.sliderHandle;
    const int radius = groove / 2;

    QStyleOptionSlider opt;
    initStyleOption(&opt);
    const bool enabled = opt.state & QStyle::State_Enabled;
    const bool hovered = opt.state & QStyle::State_MouseOver;
    const bool pressed = m_dragging;

    const QRect track = trackRect();
    const QPointF c = thumbCenter();
    const int travel = vertical ? track.height() : track.width();
    const int pos = vertical ? int(c.y()) - track.top()
                             : int(c.x()) - track.left();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QColor grooveColor = t.controlStrokeSecondary;
    QColor fill = t.handle;
    QColor border = t.handleOutline;
    if (!enabled) {
        grooveColor.setAlpha(120);
        fill = t.surface;
        border = t.controlStrokeSecondary;
    }

    p.setPen(Qt::NoPen);
    p.setBrush(grooveColor);
    p.drawRoundedRect(track, radius, radius);

    const QColor accent = enabled ? t.accent : t.textTertiary;
    // The filled sub-page runs from the start of the track to the thumb. A
    // vertical slider starts at the bottom, so the fill is anchored there and
    // grows upward. Anchoring it to the top as the horizontal case does put the
    // accent on the wrong half: the slider then reads completely filled at its
    // minimum and empty at its maximum.
    QRect sub;
    if (vertical) {
        const int y = qMin(track.top() + pos, track.bottom() - groove + 1);
        sub = QRect(track.left(), y, track.width(), track.bottom() - y + 1);
    } else {
        sub = QRect(track.left(), track.top(),
                    qBound(groove, pos, travel), track.height());
    }
    p.setBrush(accent);
    p.drawRoundedRect(sub, radius, radius);

    if (pressed) {
        fill = accent;
    } else if (hovered) {
        border = t.handleHover;
        QColor halo = accent;
        halo.setAlpha(36);
        p.setPen(Qt::NoPen);
        p.setBrush(halo);
        const double haloR = haloRadius();
        p.drawEllipse(c, haloR, haloR);
    }

    p.setPen(QPen(border, 1));
    p.setBrush(fill);
    p.drawEllipse(c, handle / 2.0, handle / 2.0);

    if (hasFocus()) {
        p.setPen(QPen(t.focusRing, 1));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(c, handle / 2.0 + 2.0, handle / 2.0 + 2.0);
    }
}