#pragma once

#include <QDialog>

class QButtonGroup;
class QCheckBox;
class QSpinBox;

// "Resize and Rotate" modal, mirroring Windows 11 Paint's dialog.
struct ResizeParams {
    bool selection = false;     // true: transform the selection
    bool usePixels = false;     // true: pixel edit / false: percentage
    int width = 100;            // pixels or percentage
    int height = 100;
    bool keepAspect = true;
    int rotateDegrees = 0;      // 90 / 180 / 270, 0 = none
};

class ResizeDialog : public QDialog {
    Q_OBJECT
public:
    ResizeDialog(const QSize& documentSize, const QSize& selectionSize,
                 QWidget* parent = nullptr);

    ResizeParams params() const;

private:
    void onHorizontalChanged();
    void onVerticalChanged();

    QButtonGroup* m_targetGroup;   // Canvas | Selection
    QButtonGroup* m_unitGroup;     // Percentage | Pixels
    QSpinBox* m_hw;
    QSpinBox* m_hwPx;
    QSpinBox* m_vw;
    QSpinBox* m_vwPx;
    QSpinBox* m_hwPct;
    QSpinBox* m_vwPct;
    QCheckBox* m_keep;
    QButtonGroup* m_rotateGroup;   // 0 / 180 / 90 / 270
    QSize m_doc;
    QSize m_selectionSize;
};