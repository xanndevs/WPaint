#pragma once

#include <QDialog>
#include <QListWidget>
#include <QStackedWidget>

class FluentSwitch;

// "Preferences" modal: a left rail of sections, a stacked page per section, and
// the usual Ok / Cancel / Restore Defaults footer.
//
// The sections are a rail rather than a tab strip because a tab bar has to fit
// four labels on one line, and these labels are sentences. The dialog stages
// its edits and applies them on accept, so Cancel is a true no-op.
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

    // Re-reads every control from the settings table.
    void reload();

private:
    QWidget* addPage(const QString& label);
    QWidget* pageContent(const QString& title, const QString& subtitle,
                         QWidget** outScrollArea);

    QListWidget* m_nav = nullptr;
    QStackedWidget* m_pages = nullptr;
    FluentSwitch* m_antialias = nullptr;
};
