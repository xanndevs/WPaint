#pragma once

#include <QComboBox>

// A QComboBox that draws its own drop-down chevron.
//
// The stylesheet's CSS-triangle trick (transparent borders with one coloured
// edge) is not something Qt's style sheet engine implements: it renders the
// three borders as a small square, which is what every combo box in the app
// showed until this existed. Pointing `image:` at an SVG is not an option
// either, because the chevron would be baked at its marker colour and stop
// following the theme. So the indicator is turned off in the sheet and the
// glyph is painted here, from the same tinted icon the toolbar carets use.
class FluentCombo : public QComboBox {
    Q_OBJECT
public:
    explicit FluentCombo(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* ev) override;
};
