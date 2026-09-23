#pragma once

#include <QColor>
#include <QDialog>

class QLabel;
class QLineEdit;
class QSpinBox;
class RgbHsvPicker;

// CV of the classic MS Paint "Edit Colors" dialog: RGB and HSV editors fed
// by an SV square + hue bar.
class ColorDialog : public QDialog {
    Q_OBJECT
public:
    explicit ColorDialog(const QColor& initial, QWidget* parent = nullptr);

    QColor chosenColor() const { return m_color; }

private:
    void syncFromPicker();
    void syncToPicker();

    RgbHsvPicker* m_picker;
    QLabel* m_swatch;
    QSpinBox* m_r;
    QSpinBox* m_g;
    QSpinBox* m_b;
    QSpinBox* m_h;
    QSpinBox* m_s;
    QSpinBox* m_v;
    QLineEdit* m_hex;
    QColor m_color;
    bool m_updating = false;
};