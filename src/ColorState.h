#pragma once

#include <QColor>
#include <QObject>
#include <QVector>

// Application-wide color selection: primary (left-click) / secondary (right-click).
class ColorState : public QObject {
    Q_OBJECT
public:
    explicit ColorState(QObject* parent = nullptr);

    const QColor& primary() const { return m_primary; }
    const QColor& secondary() const { return m_secondary; }

    void setPrimary(const QColor& c);
    void setSecondary(const QColor& c);
    // Which slot the custom color dialog edits.
    void editPrimary(bool primary);

    const QVector<QColor>& palette() const { return m_palette; }
    void setCustomPaletteColor(int index, const QColor& c);

signals:
    void colorsChanged();

private:
    QColor m_primary;
    QColor m_secondary;
    QVector<QColor> m_palette;
};