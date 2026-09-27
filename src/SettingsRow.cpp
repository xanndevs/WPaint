#include "SettingsRow.h"
#include "Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

SettingsRow::SettingsRow(const QString& title, const QString& description,
                         QWidget* parent)
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

    if (!description.isEmpty()) {
        auto* sub = new QLabel(description, this);
        sub->setObjectName("SettingsRowDesc");
        sub->setWordWrap(true);
        m_text->addWidget(sub);
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
    // Centred against the two-line text block rather than pinned to the top,
    // so a switch does not read as belonging to the heading alone.
    m_controlRow->addWidget(control, 0, Qt::AlignVCenter);
}

void SettingsRow::addContent(QWidget* content) {
    if (content) m_root->addWidget(content);
}
