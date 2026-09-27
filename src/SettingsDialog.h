#pragma once

#include <QDialog>

class QRadioButton;

// "Preferences" modal (Edit -> Preferences). Deliberately built around one
// question per setting: a setting that has good and bad values is a pair of
// radios, a setting that is simply on or off is a checkbox. Values are staged
// locally and only handed to Settings::apply() on accept, so Cancel is a true
// no-op.
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

private:
    void reload();

    QRadioButton* m_smooth;
    QRadioButton* m_crisp;
};
