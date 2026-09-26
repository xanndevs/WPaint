#pragma once

#include <QWidget>

class QSpinBox;
class QSlider;
class QWheelEvent;

// Vertical "tool size" strip to the left of the canvas, mirroring the
// thickness slider of Windows 11 Paint. The slider itself only travels 1..64,
// but the value shown under it can be typed past that (up to Tool::kMaxBrushSize)
// and the wheel keeps nudging it there; the slider is just a clamped view of
// the real value. Disabled when the active tool has no brush size.
class SizeSliderPanel : public QWidget {
    Q_OBJECT
public:
    explicit SizeSliderPanel(QWidget* parent = nullptr);

    int size() const { return m_size; }
    void setSize(int s);
    void setEnabledForTool(bool enabled);
    void setImmediateVisible(bool visible);

signals:
    void sizeChanged(int size);

protected:
    // The panel floats over the canvas scroll area, so a wheel that lands on
    // its padding or captions must not scroll the document. Only the slider
    // itself changes the value; the rest of the card just swallows the gesture.
    void wheelEvent(QWheelEvent* event) override;
    // The wheel over the slider is taken here rather than in the slider, so a
    // value already past the slider's maximum keeps moving instead of sticking.
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void apply(int value, bool notify);

    QSlider* m_slider;
    QSpinBox* m_value;
    int m_size = 4;
};
