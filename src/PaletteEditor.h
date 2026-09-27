#pragma once

#include <QVector>
#include <QWidget>

class ColorSwatchButton;

// The twenty-swatch editor that appears under the palette row when Custom is
// the selected preset.
//
// It edits a copy of the palette rather than the live setting, like every other
// control on these pages: a Cancel after twenty swatch edits should cost twenty
// swatch edits, not a palette.
class PaletteEditor : public QWidget {
    Q_OBJECT
public:
    explicit PaletteEditor(QWidget* parent = nullptr);

    void setColors(const QVector<QColor>& colors);
    QVector<QColor> colors() const;

private:
    QVector<ColorSwatchButton*> m_swatches;
    QVector<QColor> m_colors;
};
