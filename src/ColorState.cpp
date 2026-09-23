#include "ColorState.h"

ColorState::ColorState(QObject* parent) : QObject(parent) {
    m_primary = QColor("#000000");
    m_secondary = QColor("#FFFFFF");

    // Preset palette: two rows of 10, loosely modeled on the classic
    // MS Paint palette plus some Fluent-ish neutrals.
    const char* presets[] = {
        "#000000", "#7F7F7F", "#880015", "#ED1C24", "#FF7F27",
        "#FFF200", "#22B14C", "#00A2E8", "#3F48CC", "#A349A4",
        "#FFFFFF", "#C3C3C3", "#B97A57", "#FFAFAF", "#FFC90E",
        "#EFE4B0", "#B5E61D", "#99D9EA", "#7092BE", "#C8BFE7",
    };
    m_palette.reserve(20);
    for (const char* p : presets) m_palette.push_back(QColor(QLatin1String(p)));
}

void ColorState::setPrimary(const QColor& c) {
    if (!c.isValid() || c == m_primary) return;
    m_primary = c;
    emit colorsChanged();
}

void ColorState::setSecondary(const QColor& c) {
    if (!c.isValid() || c == m_secondary) return;
    m_secondary = c;
    emit colorsChanged();
}

void ColorState::editPrimary(bool primary) {
    Q_UNUSED(primary);
}

void ColorState::setCustomPaletteColor(int index, const QColor& c) {
    if (index < 0 || index >= m_palette.size()) return;
    m_palette[index] = c;
    emit colorsChanged();
}