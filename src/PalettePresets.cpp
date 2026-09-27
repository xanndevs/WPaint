#include "PalettePresets.h"

#include <QObject>

namespace {

QVector<QColor> earth() {
    return {
        QColor("#FFFFFF"), QColor("#000000"), QColor("#888888"), QColor("#A3867A"),
        QColor("#E7C69F"), QColor("#F9CB9C"), QColor("#C19171"), QColor("#8E5E3E"),
        QColor("#6E382E"), QColor("#4C301A"), QColor("#255E6B"), QColor("#5A3562"),
        QColor("#7AC7E0"), QColor("#2A7AB6"), QColor("#144E73"), QColor("#4B4B5C"),
        QColor("#90C978"), QColor("#D48AAD"), QColor("#60B04C"), QColor("#B95E2A"),
    };
}

// The classic MS Paint row, which is the set most people picture when they
// picture a paint palette.
QVector<QColor> classic() {
    return {
        QColor("#000000"), QColor("#7F7F7F"), QColor("#880015"), QColor("#ED1C24"),
        QColor("#FF7F27"), QColor("#FFF200"), QColor("#22B14C"), QColor("#00A2E8"),
        QColor("#3F48CC"), QColor("#A349A4"), QColor("#FFFFFF"), QColor("#C3C3C3"),
        QColor("#B97A57"), QColor("#FFAFAF"), QColor("#FFC90E"), QColor("#EFE4B0"),
        QColor("#B5E61D"), QColor("#99D9EA"), QColor("#7092BE"), QColor("#C8BFE7"),
    };
}

// Neutrals and a single accent ramp: the Fluent set, for people who want the
// toolbar to stay out of the way of the picture.
QVector<QColor> fluent() {
    return {
        QColor("#FFFFFF"), QColor("#F3F3F3"), QColor("#E0E0E0"), QColor("#C3C3C3"),
        QColor("#8A8A8A"), QColor("#5E5E5E"), QColor("#3B3B3B"), QColor("#1B1B1B"),
        QColor("#000000"), QColor("#005FB8"), QColor("#0067C0"), QColor("#4CC2FF"),
        QColor("#3AB4E8"), QColor("#7AC7E0"), QColor("#B5E61D"), QColor("#90C978"),
        QColor("#22B14C"), QColor("#60B04C"), QColor("#ED1C24"), QColor("#FF7F27"),
    };
}

} // namespace

namespace PalettePresets {

const QList<Preset>& all() {
    static const QList<Preset> kPresets = {
        {QObject::tr("Earth"), earth()},
        {QObject::tr("Paint"), classic()},
        {QObject::tr("Fluent"), fluent()},
        // Starts blank on purpose: twenty empty swatches read as "not set up
        // yet" in a way a copy of another preset would not.
        {QObject::tr("Custom"), QVector<QColor>(kCount, QColor("#FFFFFF"))},
    };
    return kPresets;
}

int customIndex() { return all().size() - 1; }

bool isValidIndex(int index) { return index >= 0 && index < all().size(); }

QVector<QColor> effective(int presetIndex, const QVector<QColor>& custom) {
    if (presetIndex == customIndex())
        return custom;
    if (!isValidIndex(presetIndex))
        presetIndex = 0;
    return all().at(presetIndex).colors;
}

} // namespace PalettePresets
