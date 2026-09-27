#pragma once

#include <QComboBox>
#include <QStyledItemDelegate>
#include <QVector>

// The palette dropdown: one row per preset, and each row *is* its twenty
// swatches.
//
// A list of preset names would ask the reader to remember which of four sets
// they picked. Drawing the set next to its name means the choice is the thing
// being made, the same way it is made on the toolbar.
class PalettePresetCombo : public QComboBox {
    Q_OBJECT
public:
    explicit PalettePresetCombo(QWidget* parent = nullptr);

    // One entry per preset; the swatches for the Custom row come from the
    // user's own palette, so it has to be handed in rather than read.
    void setPresets(const QStringList& names, const QVector<QVector<QColor>>& swatches);
    void setSelected(int index);
    int selected() const;

protected:
    // The closed combo shows the current palette too, not just its name: the row
    // is about which colours the toolbar will have, so the colours belong on the
    // row whether the list is open or not.
    void paintEvent(QPaintEvent* ev) override;

private:
    QVector<QVector<QColor>> m_swatches;
};
