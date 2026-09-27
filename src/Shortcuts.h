#pragma once

#include <QKeySequence>
#include <QList>
#include <QMap>
#include <QString>

// Every rebindable keyboard shortcut in the app, in one table.
//
// A shortcut is stored against a stable id rather than against the QAction that
// happens to exist this week, for two reasons. The Shortcuts page needs a label
// and a group to draw, and a menu-less action -- the flip buttons, the shape
// gallery entries -- has no menu row to hang a label off. And a stored binding
// has to survive being rebound: if the key were the QAction's text, renaming
// "Zoom In" would orphan everybody's binding.
//
// Gestures that are not key sequences (Space + wheel, middle-drag, the arrow
// keys) are listed too, but flagged as gestures: the page shows them so the
// behaviour is documented in one place, and they cannot be rebound because
// there is no QKeySequence to rebind.
namespace Shortcuts {

enum class Group { File, Edit, View, Tools, Image, Shapes, Brushes, Layers, Navigation };

struct Entry {
    const char* id;        // stable, persisted; never reuse or renumber
    Group group;
    const char* label;     // what the row says
    const char* sequence;  // default key sequence, empty for a gesture
    const char* gesture;   // what a non-rebindable gesture looks like
    bool isGesture = false;
};

const QList<Entry>& entries();
const Entry* find(const QString& id);
QString groupName(Group g);
QList<Group> groups();

// The stored override for `id`, or an empty string when the default stands.
QString overrideFor(const QString& id);
void setOverride(const QString& id, const QString& sequence);

// The sequence to bind: the override if there is one, else the default. Returns
// an empty sequence for a gesture, and for a gesture `*outGesture` is set to
// the text the Shortcuts page shows instead.
QKeySequence effective(const Entry& e, QString* outGesture = nullptr);

// Human-readable form, or an em dash when unbound. Stored as text rather than as
// a QKeySequence because QKeySequence::Preferences is empty on this platform,
// so a binding has to be spellable and re-parseable as a string.
QString describe(const QKeySequence& seq);

// Every id currently bound to `seq`, excluding `exceptId`. Used to tell the user
// a binding is already taken rather than silently stealing it.
QList<QString> conflictsWith(const QKeySequence& seq, const QString& exceptId);

// A text sequence round-trips through QKeySequence, so an override written by an
// older build and a default written today compare equal as strings.
QString toText(const QKeySequence& seq);
bool fromText(const QString& text, QKeySequence& out);

} // namespace Shortcuts
