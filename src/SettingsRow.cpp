#include "SettingsRow.h"
#include "Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

SettingsRow::SettingsRow(const QString& title, const QString& description,
                         QWidget* parent, Description desc)
    : QWidget(parent) {
    setObjectName("SettingsRow");

    m_root = new QVBoxLayout(this);
    m_root->setContentsMargins(0, 0, 0, 0);
    m_root->setSpacing(Theme::tokens().gap);

    auto* line = new QHBoxLayout;
    line->setContentsMargins(0, 0, 0, 0);
    line->setSpacing(Theme::tokens().gap * 4);

    m_text = new QVBoxLayout;
    m_text->setContentsMargins(0, 0, 0, 0);
    m_text->setSpacing(1);

    auto* heading = new QLabel(title, this);
    heading->setObjectName("SettingsRowTitle");
    m_text->addWidget(heading);

    // The description is a tooltip, not a second line. A page of twenty rows
    // with two lines each is a wall of grey prose nobody reads, and the second
    // line is only interesting to the one person who is wondering what a
    // setting does -- which is exactly what hovering is for. The row is one
    // click target and the control sits on the same line as its name, so the
    // whole page reads as a list of settings rather than a document.
    if (!description.isEmpty()) {
        m_description = description;
        setToolTip(description);
        heading->setToolTip(description);
        if (desc == Description::Inline) {
            // For the one or two settings on a page where the explanation is the
            // point: a swatch with no picture of itself, or a preset the reader
            // has to guess at.
            auto* sub = new QLabel(description, this);
            sub->setObjectName("SettingsRowDesc");
            sub->setWordWrap(true);
            m_text->addWidget(sub);
        }
    }

    line->addLayout(m_text, 1);
    m_controlRow = new QHBoxLayout;
    m_controlRow->setContentsMargins(0, 0, 0, 0);
    m_controlRow->addStretch(1);
    line->addLayout(m_controlRow, 0);

    m_root->addLayout(line);
}

void SettingsRow::setControl(QWidget* control) {
    if (!control) return;
    // Centred against the title rather than pinned to the top, so a switch does
    // not read as belonging to the heading alone.
    m_controlRow->addWidget(control, 0, Qt::AlignVCenter);
    adoptTooltip(control);
}

void SettingsRow::addControl(QWidget* control) {
    if (!control) return;
    m_controlRow->addWidget(control, 0, Qt::AlignVCenter);
    adoptTooltip(control);
}

void SettingsRow::addContent(QWidget* content) {
    if (content) m_root->addWidget(content);
}

void SettingsRow::addContentLayout(QLayout* layout) {
    if (layout) m_root->addLayout(layout);
}

void SettingsRow::adoptTooltip(QWidget* control) {
    if (m_description.isEmpty())
        return;
    // A control that has something more specific to say keeps it; otherwise it
    // inherits the row's, so hovering a switch explains its setting the same as
    // hovering its name does.
    if (control->toolTip().isEmpty() || control->toolTip() == m_description)
        control->setToolTip(m_description);
}
