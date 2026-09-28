#pragma once

#include <QDialog>

// What to do with an image dropped on a canvas that already has something on it.
//
// A drop on a document you have not touched needs no question -- there is a
// preference for that. This is the other case: three answers, and the first one
// throws work away, so it is not the default and not the first thing pressed.
class FileDropDialog : public QDialog {
    Q_OBJECT
public:
    enum class Choice {
        None,             // cancelled: the drop does nothing
        DiscardAndOpen,   // open the file as the document, losing what is there
        NewWindow,        // open the file in a window of its own
        PlaceToLayer,     // put the image on a new layer over what is there
    };

    FileDropDialog(const QString& fileName, QWidget* parent = nullptr);

    Choice choice() const { return m_choice; }

private:
    Choice m_choice = Choice::None;
};
