#pragma once

#include "Shortcuts.h"

#include <QHash>
#include <QWidget>

class QCheckBox;
class QLineEdit;
class QVBoxLayout;
class ShortcutEdit;
class QToolButton;

// The Shortcuts page: every rebindable key, grouped, plus the gestures that
// cannot be rebound.
//
// Rows are rebuilt whenever the filter or the gesture toggle changes rather than
// hidden and shown. A row is four widgets and three lambdas, and fifty of them
// kept in sync by a filter is where the bugs go.
class SettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPage(QWidget* parent = nullptr);

    // Re-read every row from the table.
    void reload();

private:
    struct Row {
        ShortcutEdit* edit = nullptr;
        QToolButton* reset = nullptr;
    };
    void rebuild();
    QWidget* buildRow(const Shortcuts::Entry* e);

    QLineEdit* m_filter = nullptr;
    QCheckBox* m_gestures = nullptr;
    QWidget* m_body = nullptr;
    QVBoxLayout* m_bodyLayout = nullptr;
    QHash<QString, Row> m_rows;
};
