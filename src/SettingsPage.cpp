#include "SettingsPage.h"
#include "SettingsRow.h"
#include "ShortcutEdit.h"
#include "Shortcuts.h"
#include "Theme.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

SettingsPage::SettingsPage(QWidget* parent) : QWidget(parent) {
    setObjectName("SettingsPage");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(2);

    auto* heading = new QLabel(tr("Keyboard"), this);
    heading->setObjectName("SettingsPageTitle");
    root->addWidget(heading);
    auto* subtitle = new QLabel(
        tr("Click a key and press the combination you want. The Shortcuts page "
           "stores only what you change, so a new default in a later version "
           "still reaches you."),
        this);
    subtitle->setObjectName("SettingsPageSub");
    subtitle->setWordWrap(true);
    root->addWidget(subtitle);

    auto* tools = new QHBoxLayout;
    tools->setContentsMargins(0, 0, 0, 0);
    tools->setSpacing(Theme::tokens().gap * 2);
    m_filter = new QLineEdit(this);
    m_filter->setObjectName("SettingsFilter");
    m_filter->setPlaceholderText(tr("Search the keys"));
    m_filter->setClearButtonEnabled(true);
    m_filter->setMaximumWidth(280);
    tools->addWidget(m_filter);
    m_gestures = new QCheckBox(tr("Show gestures that cannot be rebound"), this);
    m_gestures->setChecked(true);
    tools->addWidget(m_gestures);
    tools->addStretch(1);
    root->addLayout(tools);

    auto* scroll = new QScrollArea(this);
    scroll->setObjectName("SettingsScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_body = new QWidget;
    m_body->setObjectName("SettingsPage");
    m_bodyLayout = new QVBoxLayout(m_body);
    m_bodyLayout->setContentsMargins(0, 4, Theme::tokens().pad, Theme::tokens().pad * 2);
    m_bodyLayout->setSpacing(2);
    scroll->setWidget(m_body);
    root->addWidget(scroll, 1);

    connect(m_filter, &QLineEdit::textChanged, this, &SettingsPage::rebuild);
    connect(m_gestures, &QCheckBox::toggled, this, &SettingsPage::rebuild);
    rebuild();
}

void SettingsPage::reload() { rebuild(); }

void SettingsPage::rebuild() {
    // Rebuilding rather than hiding: a row is four widgets and three lambdas,
    // and a filter that has to keep fifty of them in sync is where the bugs go.
    while (m_bodyLayout->count() > 0) {
        QLayoutItem* item = m_bodyLayout->takeAt(0);
        if (QWidget* w = item->widget())
            w->deleteLater();
        delete item;
    }
    m_rows.clear();

    const QString needle = m_filter->text().trimmed();
    for (Shortcuts::Group group : Shortcuts::groups()) {
        QList<const Shortcuts::Entry*> shown;
        for (const Shortcuts::Entry& e : Shortcuts::entries()) {
            if (e.group != group)
                continue;
            if (e.isGesture && !m_gestures->isChecked())
                continue;
            const QString label = QCoreApplication::translate("Shortcuts", e.label);
            if (!needle.isEmpty() && !label.contains(needle, Qt::CaseInsensitive) &&
                !QLatin1String(e.id).contains(needle, Qt::CaseInsensitive))
                continue;
            shown << &e;
        }
        if (shown.isEmpty())
            continue;

        auto* header = new QLabel(Shortcuts::groupName(group), m_body);
        header->setObjectName("SettingsSection");
        m_bodyLayout->addWidget(header);

        for (const Shortcuts::Entry* e : shown)
            m_bodyLayout->addWidget(buildRow(e));
    }
    m_bodyLayout->addStretch(1);
}

QWidget* SettingsPage::buildRow(const Shortcuts::Entry* e) {
    const QString id = QLatin1String(e->id);
    const QString label = QCoreApplication::translate("Shortcuts", e->label);

    auto* row = new SettingsRow(label,
                                e->isGesture
                                    ? tr("Built in. Listed here so every gesture lives "
                                         "in one place.")
                                    : QString(),
                                m_body);
    row->setProperty("wpShortcutId", id);

    auto* edit = new ShortcutEdit(row);
    QString gestureText;
    const QKeySequence seq = Shortcuts::effective(*e, &gestureText);
    edit->setSequence(seq);
    if (e->isGesture) {
        edit->setGesture(true);
        edit->setGestureText(gestureText);
    }
    row->addControl(edit);

    auto* reset = new QToolButton(row);
    reset->setObjectName("ShortcutReset");
    reset->setText(tr("Reset"));
    reset->setToolTip(tr("Put this back to its default"));
    reset->setProperty("wpFlat", 1);
    // Only a row that has been changed needs a way back, and fifty always-present
    // Reset buttons is noise.
    const bool modified = !e->isGesture && !Shortcuts::overrideFor(id).isEmpty();
    reset->setVisible(modified);
    row->addControl(reset);

    auto* warning = new QLabel(row);
    warning->setObjectName("ShortcutWarning");
    warning->setWordWrap(true);
    warning->hide();
    row->addControl(warning);

    connect(edit, &ShortcutEdit::sequenceCaptured, this,
            [this, id, warning, reset, edit](const QKeySequence& captured) {
                const QList<QString> taken = Shortcuts::conflictsWith(captured, id);
                if (!taken.isEmpty()) {
                    const Shortcuts::Entry* other = Shortcuts::find(taken.first());
                    warning->setText(
                        tr("Already used by %1. Both keys will do the same thing.")
                            .arg(QCoreApplication::translate("Shortcuts", other->label)));
                    warning->show();
                    const Shortcuts::Entry* self = Shortcuts::find(id);
                    if (self)
                        edit->setSequence(Shortcuts::effective(*self));
                    return;
                }
                warning->hide();
                Shortcuts::setOverride(id, Shortcuts::toText(captured));
                reset->setVisible(!captured.isEmpty());
            });

    connect(reset, &QToolButton::clicked, this, [this, id, edit, warning, reset] {
        Shortcuts::setOverride(id, QString());
        const Shortcuts::Entry* entry = Shortcuts::find(id);
        edit->setSequence(entry ? Shortcuts::effective(*entry) : QKeySequence());
        warning->hide();
        reset->hide();
    });

    m_rows.insert(id, {edit, reset});
    return row;
}
