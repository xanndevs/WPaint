#pragma once

#include <QColor>
#include <QWidget>

// A colour swatch that opens the editor when clicked and paints itself edge to
// edge. A QPushButton cannot do this job: the generic tool-button stylesheet
// inflates every styled instance, and the colour has to *be* the button rather
// than sit inside one.
class ColorSwatchButton : public QWidget {
    Q_OBJECT
public:
    ColorSwatchButton(const QColor& color, const QString& title,
                      QWidget* parent = nullptr);

    QColor color() const { return m_color; }
    void setColor(const QColor& c);

signals:
    void picked();

protected:
    void paintEvent(QPaintEvent* ev) override;
    void mouseReleaseEvent(QMouseEvent* ev) override;

private:
    QColor m_color;
};
