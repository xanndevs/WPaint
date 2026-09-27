#include "SettingsDialog.h"
#include "Settings.h"

#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Preferences"));
    setModal(true);
    setMinimumWidth(380);

    auto* renderBox = new QGroupBox(tr("Image rendering"), this);

    m_smooth = new QRadioButton(tr("Smooth edges (antialiased)"), renderBox);
    m_smooth->setToolTip(tr("Blend pixels when the canvas is magnified or shrunk"));
    m_crisp = new QRadioButton(tr("Crisp pixels (nearest neighbour)"), renderBox);
    m_crisp->setToolTip(tr("Keep every document pixel a hard-edged block when zoomed in"));
    reload();

    auto* renderLayout = new QVBoxLayout(renderBox);
    renderLayout->addWidget(m_smooth);
    renderLayout->addWidget(m_crisp);

    auto* hint = new QLabel(
        tr("Affects how the document is drawn on screen only. Shapes and text are\n"
           "still drawn with smooth edges, and saved files are unchanged."),
        this);
    hint->setObjectName("PreferencesHint");
    hint->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults,
        this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        SettingsValues v;
        v.antialiasCanvas = m_smooth->isChecked();
        Settings::apply(v);
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
    connect(buttons, &QDialogButtonBox::clicked, this, [this, buttons](QAbstractButton* b) {
        if (buttons->buttonRole(b) != QDialogButtonBox::ResetRole)
            return;
        Settings::resetDefaults();
        reload();
    });

    auto* root = new QVBoxLayout(this);
    root->addWidget(renderBox);
    root->addWidget(hint);
    auto* row = new QHBoxLayout;
    row->addStretch(1);
    row->addWidget(buttons);
    root->addLayout(row);
}

void SettingsDialog::reload() {
    m_smooth->setChecked(Settings::antialiasCanvas());
    m_crisp->setChecked(!Settings::antialiasCanvas());
}
