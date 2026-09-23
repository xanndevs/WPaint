#pragma once

#include <QWidget>

class QLabel;
class QSlider;

// Vertical "tool size" strip to the left of the canvas, mirroring the
// thickness slider of Windows 11 Paint. 1..64 px, disabled when the active
// tool has no brush size.
class SizeSliderPanel : public QWidget {
    Q_OBJECT
public:
    explicit SizeSliderPanel(QWidget* parent = nullptr);

    int size() const;
    void setSize(int s); // set without emitting
    void setEnabledForTool(bool enabled);
    void setImmediateVisible(bool visible);

signals:
    void sizeChanged(int size);

private:
    QSlider* m_slider;
    QLabel* m_value;
};