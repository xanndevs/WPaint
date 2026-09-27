#pragma once

#include <QAbstractButton>

class QPaintEvent;

// A Fluent toggle switch: a pill track with a thumb that slides across it.
//
// Painted rather than styled. A QSS switch on QCheckBox is a checkbox with a
// wide border-radius -- the thumb has to be a sub-control image, and a
// stylesheet cannot slide one from one end of the box to the other, so the
// whole thing sits still in whichever state the ::indicator rules describe.
// Painting it is the same trade the custom-painted slider and the split
// gallery buttons already make: a couple of token reads and a rounded rect.
class FluentSwitch : public QAbstractButton {
    Q_OBJECT
public:
    explicit FluentSwitch(QWidget* parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent* ev) override;
    void keyPressEvent(QKeyEvent* ev) override;
};
