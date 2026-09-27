#pragma once

#include "PaletteEditor.h"
#include "PalettePresetCombo.h"
#include "Settings.h"
#include "SettingsRow.h"

#include <QDialog>
#include <QHash>
#include <QList>
#include <QListWidget>
#include <QStackedWidget>
#include <functional>

class FluentSwitch;
class QComboBox;
class QSpinBox;
class SettingsPage;
class SettingsRow;

// "Preferences" modal: a left rail of sections, a stacked page per section, and
// the usual Ok / Cancel / Restore Defaults footer.
//
// The sections are a rail rather than a tab strip because a tab bar has to fit
// four labels on one line, and these labels are sentences. The dialog stages
// its edits and applies them on accept, so Cancel is a true no-op.
//
// Every page is built from a flat list of RowSpec. All the wording -- the
// sentence that names a setting and the one that explains it -- lives in that
// list, at the top of the page's builder, because prose is the part of a
// preferences dialog that gets edited most and is the part a layout statement
// buries worst.
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

    void reload();

    // A row's explanation is a tooltip by default; Inline is for the one or two
    // settings on a page where the explanation is the point and a hover would be
    // too easy to miss. See SettingsRow::Description.
    using Description = SettingsRow::Description;

    struct RowSpec {
        const char* section; // the heading this row sits under; "" continues the last
        const char* id;      // key into the control map
        const char* title;   // the sentence
        const char* description;
        Description desc = Description::Tooltip;
        // Builds the control, parents it to the row, and returns it.
        std::function<QWidget*(SettingsRow*)> make;
        // Reads the control back into the values, on accept.
        std::function<void(SettingsValues&, QWidget*)> write;
    };

private:
    void buildPage(const QList<RowSpec>& specs, const QString& title,
                   const QString& subtitle);
    void buildBehaviorPage();
    void buildDefaultsPage();
    void buildShortcutsPage();
    void buildAboutPage();

    QListWidget* m_nav = nullptr;
    QStackedWidget* m_pages = nullptr;
    SettingsPage* m_shortcuts = nullptr;
    QHash<QString, QWidget*> m_controls;
    QHash<QString, std::function<void(SettingsValues&, QWidget*)>> m_writers;
};
