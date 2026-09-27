#include "SettingsDialog.h"
#include "FluentSwitch.h"
#include "Settings.h"
#include "SettingsRow.h"
#include "Theme.h"

#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

// A page is a title, a one-line explanation of what the section is for, and then
// as much scrolling content as it needs. Only the content scrolls, so the
// section heading stays put while a long page is scrolled.
QWidget* makePage(const QString& title, const QString& subtitle, QVBoxLayout** outBody) {
    const auto& t = Theme::tokens();
    auto* page = new QWidget;
    page->setObjectName("SettingsPage");
    auto* root = new QVBoxLayout(page);
    // Breathing room on the left of the text column, so the page heading does
    // not sit flush against the rail it belongs to.
    root->setContentsMargins(t.pad * 3, t.pad, 0, 0);
    root->setSpacing(2);

    auto* heading = new QLabel(title, page);
    heading->setObjectName("SettingsPageTitle");
    root->addWidget(heading);

    if (!subtitle.isEmpty()) {
        auto* sub = new QLabel(subtitle, page);
        sub->setObjectName("SettingsPageSub");
        sub->setWordWrap(true);
        root->addWidget(sub);
    }

    auto* scroll = new QScrollArea(page);
    scroll->setObjectName("SettingsScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* body = new QWidget;
    body->setObjectName("SettingsPage");
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(0, 4, t.pad, t.pad * 2);
    bodyLayout->setSpacing(2);
    scroll->setWidget(body);
    root->addWidget(scroll, 1);

    *outBody = bodyLayout;
    return page;
}

QLabel* sectionHeader(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setObjectName("SettingsSection");
    return label;
}

} // namespace

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Preferences"));
    setModal(true);
    resize(880, 600);
    setMinimumSize(680, 480);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* header = new QWidget(this);
    auto* headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(Theme::tokens().pad * 2, Theme::tokens().pad * 2,
                                     Theme::tokens().pad * 2, Theme::tokens().pad);
    headerLayout->setSpacing(2);
    auto* title = new QLabel(tr("Preferences"), header);
    title->setObjectName("DialogHeader");
    auto* subtitle = new QLabel(
        tr("WPaint %1 — how the editor behaves, what a new document starts as, "
           "and what every key does.")
            .arg(QLatin1String(WP_VERSION)),
        header);
    subtitle->setObjectName("PreferencesHint");
    headerLayout->addWidget(title);
    headerLayout->addWidget(subtitle);
    root->addWidget(header);

    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);

    m_nav = new QListWidget(this);
    m_nav->setObjectName("SettingsNav");
    m_nav->setFixedWidth(Theme::tokens().settingsNavW);
    m_nav->setFrameShape(QFrame::NoFrame);
    m_nav->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    body->addWidget(m_nav);

    m_pages = new QStackedWidget(this);
    body->addWidget(m_pages, 1);
    root->addLayout(body, 1);

    // ---- Behavior ----
    {
        QVBoxLayout* page = nullptr;
        QWidget* w = makePage(tr("Behavior"),
                              tr("How the editor responds while you work. None of "
                                 "these change what gets saved."),
                              &page);
        page->addWidget(sectionHeader(tr("Canvas"), w));
        auto* row = new SettingsRow(
            tr("Antialias the canvas"),
            tr("Blend pixels as the document is scaled, so magnified edges stay "
               "smooth instead of blocky. Shapes and text keep their own smooth "
               "edges either way, and saved files are never affected."),
            w);
        m_antialias = new FluentSwitch(row);
        row->setControl(m_antialias);
        page->addWidget(row);
        page->addStretch(1);
        m_pages->addWidget(w);
        m_nav->addItem(tr("Behavior"));
    }

    // ---- About ----
    {
        QVBoxLayout* page = nullptr;
        QWidget* w = makePage(tr("About"), QString(), &page);
        auto* blurb = new QLabel(
            tr("WPaint is a layered image editor that borrows its layout from "
               "Windows 11 Paint: grouped toolbars, a layers rail and a size "
               "strip, with the document as a stack of images rather than a "
               "single flattened bitmap."),
            w);
        blurb->setObjectName("SettingsAboutBlurb");
        blurb->setWordWrap(true);
        page->addWidget(blurb);

        page->addWidget(sectionHeader(tr("Version"), w));
        for (const QString& line : {tr("WPaint %1").arg(QLatin1String(WP_VERSION)),
                                    tr("Built with Qt %1").arg(QLatin1String(qVersion()))}) {
            auto* value = new QLabel(line, w);
            value->setObjectName("SettingsKeyValue");
            page->addWidget(value);
        }
        page->addStretch(1);
        m_pages->addWidget(w);
        m_nav->addItem(tr("About"));
    }

    connect(m_nav, &QListWidget::currentRowChanged, m_pages, &QStackedWidget::setCurrentIndex);
    m_nav->setCurrentRow(0);

    auto* hint = new QLabel(
        tr("Changes take effect as soon as you press OK. Cancel leaves everything "
           "as it was."),
        this);
    hint->setObjectName("PreferencesHint");
    hint->setContentsMargins(Theme::tokens().pad * 2, 0, 0, 0);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults,
        this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        SettingsValues v = Settings::values();
        v.antialiasCanvas = m_antialias->isChecked();
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

    auto* footer = new QHBoxLayout;
    footer->setContentsMargins(0, 0, Theme::tokens().pad * 2, Theme::tokens().pad * 2);
    footer->addWidget(hint, 1);
    footer->addWidget(buttons);
    root->addLayout(footer);

    reload();
}

void SettingsDialog::reload() {
    m_antialias->setChecked(Settings::antialiasCanvas());
}
