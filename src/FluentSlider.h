#pragma once

#include <QSlider>

class QMouseEvent;

// QSlider with a self-drawn Fluent-style round thumb and accent sub-page.
// QSS cannot produce a round handle for sliders (the cross-axis size is
// clamped to the groove), so the whole control is custom-painted using
// Theme::tokens(). Mouse handling is also custom so the drawn thumb (which
// sticks out past the groove) is grabbable and the track drags; works in both
// orientations.
class FluentSlider : public QSlider {
    Q_OBJECT
public:
    explicit FluentSlider(Qt::Orientation orientation, QWidget* parent = nullptr);
    QSize sizeHint() const override;
    QRect trackRect() const;
    QPointF thumbCenter() const;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    int valueForPoint(const QPointF& p) const;

    bool m_dragging = false;
};