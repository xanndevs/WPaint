#include "ShortcutEdit.h"
#include "Shortcuts.h"

#include <QKeyEvent>

ShortcutEdit::ShortcutEdit(QWidget* parent) : QLineEdit(parent) {
    setObjectName("ShortcutKey");
    setReadOnly(true);
    setAlignment(Qt::AlignCenter);
    setCursor(Qt::PointingHandCursor);
    setContextMenuPolicy(Qt::NoContextMenu);
    setMinimumWidth(120);
}

void ShortcutEdit::setSequence(const QKeySequence& seq) {
    m_sequence = seq;
    refresh();
}

void ShortcutEdit::setGesture(bool gesture) {
    m_gesture = gesture;
    refresh();
}

void ShortcutEdit::setGestureText(const QString& text) {
    setPlaceholderText(text);
    refresh();
}

void ShortcutEdit::refresh() {
    if (m_gesture) {
        setText(placeholderText());
        setToolTip(tr("This is a gesture rather than a key, so it cannot be rebound."));
        setFocusPolicy(Qt::NoFocus);
        return;
    }
    setFocusPolicy(Qt::StrongFocus);
    setText(Shortcuts::describe(m_sequence));
    setToolTip(m_sequence.isEmpty()
                   ? tr("Press a key or combination. Backspace clears it.")
                   : tr("%1. Press a new combination to change it, or Backspace to "
                        "clear it.")
                         .arg(Shortcuts::describe(m_sequence)));
}

void ShortcutEdit::keyPressEvent(QKeyEvent* ev) {
    if (m_gesture) {
        ev->ignore();
        return;
    }
    if (ev->key() == Qt::Key_Backspace || ev->key() == Qt::Key_Delete) {
        setSequence({});
        emit sequenceCaptured({});
        ev->accept();
        return;
    }
    // A bare modifier is not a binding: nobody means "just Shift", and storing
    // it would make the next real keypress look like a conflict.
    if (ev->key() == Qt::Key_Shift || ev->key() == Qt::Key_Control ||
        ev->key() == Qt::Key_Alt || ev->key() == Qt::Key_Meta) {
        ev->accept();
        return;
    }
    const QKeySequence seq(ev->key() | ev->modifiers());
    if (seq.isEmpty()) {
        ev->ignore();
        return;
    }
    setSequence(seq);
    emit sequenceCaptured(seq);
    ev->accept();
}

void ShortcutEdit::focusInEvent(QFocusEvent* ev) {
    QLineEdit::focusInEvent(ev);
    // Select-all would invite a paste; the field is a key cap, not a text box.
    selectAll();
}

void ShortcutEdit::focusOutEvent(QFocusEvent* ev) {
    QLineEdit::focusOutEvent(ev);
}
