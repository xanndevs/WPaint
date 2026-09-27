#pragma once

#include <QColor>
#include <QList>
#include <QVector>

// The colour palette, as a preference.
//
// Twenty swatches, always, in one order -- the same 10x2 grid the toolbar shows
// -- because the grid is positional: swatch 7 is a position, not a colour, and
// a palette that could be a different length would leave the last row of the
// toolbar unpainted rather than showing a default.
//
// There are three presets and one slot for the user's own. "Custom" is a preset
// like any other, it just starts as twenty blanks; picking it in Preferences
// reveals an editor, which is the only way to change it. That is deliberate: a
// palette someone can only reach by right-clicking the toolbar is a palette
// most people will never find.
namespace PalettePresets {

// The number of swatches. Not a constant anybody should change: the toolbar lays
// them out two rows of ten.
constexpr int kCount = 20;
constexpr int kColumns = 10;

struct Preset {
    QString name;
    QVector<QColor> colors;
};

// Every preset, in the order the dropdown lists them. The last one is Custom and
// is always the user's own; the editor is what fills it.
const QList<Preset>& all();

// Index of the Custom entry.
int customIndex();

bool isValidIndex(int index);

// The palette the toolbar should draw: the chosen preset, or the user's own
// when that is what is selected. Falls back to the first preset for an index a
// stale config points at.
QVector<QColor> effective(int presetIndex, const QVector<QColor>& custom);

} // namespace PalettePresets
