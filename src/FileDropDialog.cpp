#include "FileDropDialog.h"

#include "Theme.h"

#include <QDialogButtonBox>
#include <QFileInfo>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

FileDropDialog::FileDropDialog(const QString& fileName, QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("An image was dropped"));
    setModal(true);
    setObjectName("FileDropDialog");
    setMinimumWidth(420);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(Theme::tokens().pad * 2, Theme::tokens().pad * 2,
                             Theme::tokens().pad * 2, Theme::tokens().pad);
    root->setSpacing(Theme::tokens().gap * 2);

    auto* heading = new QLabel(tr("What should WPaint do with the image?"), this);
    heading->setObjectName("DialogHeader");
    root->addWidget(heading);

    auto* sub = new QLabel(
        tr("%1 was dropped on an image that already has something on it.")
            .arg(QFileInfo(fileName).fileName()),
        this);
    sub->setObjectName("HintLabel");
    sub->setWordWrap(true);
    root->addWidget(sub);

    auto* box = new QDialogButtonBox(this);
    // In the order of how often they are wanted, with the destructive one last
    // rather than first: it throws work away, so it must not be what a
    // double-click or a stray Return lands on.
    auto* place = box->addButton(tr("Place to a New Layer"), QDialogButtonBox::AcceptRole);
    auto* newWindow = box->addButton(tr("New Window"), QDialogButtonBox::ActionRole);
    // Built rather than added by text, so the object name is set before the
    // widget is ever polished: a name set afterwards is not seen by the style
    // sheet's cache, and the button comes out an ordinary one.
    // The danger token, which until now had no consumer in the app at all: a
    // button that loses work says so in colour, not only in its wording.
    auto* discard = new QPushButton(tr("Discard Changes && Open"));
    discard->setObjectName("DangerButton");
    box->addButton(discard, QDialogButtonBox::DestructiveRole);
    box->addButton(QDialogButtonBox::Cancel);
    root->addWidget(box);

    connect(place, &QPushButton::clicked, this, [this] {
        m_choice = Choice::PlaceToLayer;
        accept();
    });
    connect(newWindow, &QPushButton::clicked, this, [this] {
        m_choice = Choice::NewWindow;
        accept();
    });
    connect(discard, &QPushButton::clicked, this, [this] {
        m_choice = Choice::DiscardAndOpen;
        accept();
    });
    connect(box, &QDialogButtonBox::rejected, this, [this] {
        m_choice = Choice::None;
        reject();
    });
}
