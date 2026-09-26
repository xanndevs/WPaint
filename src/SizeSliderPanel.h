#pragma once

#include <QWidget>

class QLabel;
class QSlider;
class QWheelEvent;

// Vertical "tool size" strip to the left of the canvas, mirroring the
// thickness slider of Windows 11 Paint. 1..64 px, disabled when the active
// tool has no brush size.
class SizeSliderPanel : public QWidget {
    Q_OBJECT
public:
    explicit SizeSliderPanel(QWidget* parent = nullptr);

    int size() const;
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

private:
    QSlider* m_slider;
    QLabel* m_value;
};