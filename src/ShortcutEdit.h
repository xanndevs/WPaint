#pragma once

#include <QKeySequence>
#include <QLineEdit>

// The key-cap field on the Shortcuts page.
//
// A QKeySequenceEdit would do the parsing, but it arrives as a line edit with
// its own clear button and a context menu, neither of which belongs on a row
// that has its own reset control, and it cannot refuse a combination that is
// already taken. So this is a read-only line edit that captures the next
// key press, reports it, and lets the page decide whether to keep it.
class ShortcutEdit : public QLineEdit {
    Q_OBJECT
public:
    explicit ShortcutEdit(QWidget* parent = nullptr);

    QKeySequence sequence() const { return m_sequence; }
    void setSequence(const QKeySequence& seq);
    void setGestureText(const QString& text);
    // A gesture row has nothing to capture, so the field stops taking focus and
    // says why.
    void setGesture(bool gesture);

signals:
    void sequenceCaptured(const QKeySequence& sequence);

protected:
    void keyPressEvent(QKeyEvent* ev) override;
    void focusInEvent(QFocusEvent* ev) override;
    void focusOutEvent(QFocusEvent* ev) override;

private:
    void refresh();

    QKeySequence m_sequence;
    bool m_gesture = false;
};
