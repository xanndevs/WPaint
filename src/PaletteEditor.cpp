#include "PaletteEditor.h"

#include "PalettePresets.h"
#include "Settings.h"
#include "SettingsSwatchButton.h"
#include "Theme.h"

#include <QColorDialog>
#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>

// The 10x2 editor that appears under the palette row once Custom is selected.
//
// It edits a copy: the live settings value only changes when the dialog is
// accepted, like everything else on these pages, so a Cancel after twenty swatch
// edits costs twenty swatch edits and not a palette.
PaletteEditor::PaletteEditor(QWidget* parent) : QWidget(parent) {
    setObjectName("PaletteEditor");
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(Theme::tokens().gap);

    auto* grid = new QGridLayout;
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(Theme::tokens().gap);
    // Left-aligned and no stretch: the swatches are a fixed size, and letting the
    // grid hand them the whole row would spread twenty small squares across the
    // page instead of showing them as the toolbar's 10x2 block.
    grid->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    for (int i = 0; i < PalettePresets::kCount; ++i) {
        auto* swatch = new ColorSwatchButton(QColor(Qt::white), tr("Swatch %1").arg(i + 1), this);
        swatch->setFixedSize(28, 28);
        connect(swatch, &ColorSwatchButton::picked, swatch, [this, swatch, i] {
            const QColor chosen = QColorDialog::getColor(swatch->color(), swatch,
                                                         tr("Swatch %1").arg(i + 1));
            if (!chosen.isValid())
                return;
            m_colors[i] = chosen;
            swatch->setColor(chosen);
        });
        m_swatches.append(swatch);
        grid->addWidget(swatch, i / PalettePresets::kColumns, i % PalettePresets::kColumns);
    }
    root->addLayout(grid);

    auto* hint = new QLabel(
        tr("Click a swatch to change it. Transparent is available from the colour "
           "editor, and a palette that has never been touched is twenty whites."),
        this);
    hint->setObjectName("SettingsRowDesc");
    hint->setWordWrap(true);
    root->addWidget(hint);
}

void PaletteEditor::setColors(const QVector<QColor>& colors) {
    m_colors = colors;
    while (m_colors.size() < PalettePresets::kCount)
        m_colors.append(QColor(Qt::white));
    for (int i = 0; i < m_swatches.size() && i < m_colors.size(); ++i)
        m_swatches.at(i)->setColor(m_colors.at(i));
}

QVector<QColor> PaletteEditor::colors() const { return m_colors; }
