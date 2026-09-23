#include "CopilotPanel.h"
#include "Theme.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

CopilotPanel::CopilotPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("CopilotPanel");
    setMinimumWidth(Theme::tokens().panelW);

    auto* headerIcon = new QLabel(this);
    headerIcon->setPixmap(Theme::icon("copilot", 26).pixmap(26, 26));

    auto* headerText = new QLabel(tr("Draw with Copilot"), this);
    headerText->setObjectName("CopilotTitle");

    auto* header = new QHBoxLayout;
    header->addWidget(headerIcon);
    header->addWidget(headerText);
    header->addStretch(1);

    auto* badge = new QLabel(tr("Preview"), this);
    badge->setObjectName("CopilotBadge");
    header->addWidget(badge);

    m_chat = new QTextBrowser(this);
    m_chat->setObjectName("CopilotChat");
    m_chat->setOpenExternalLinks(false);
    m_chat->setHtml(tr(
        "<div class='copilot-bubble assistant'>"
        "<b>Copilot</b><br>"
        "I'm not connected in this preview build yet. I'll sit here and "
        "look pretty until the model is wired up."
        "</div>"));

    m_input = new QLineEdit(this);
    m_input->setObjectName("CopilotInput");
    m_input->setPlaceholderText(tr("Ask Copilot to change your image"));
    m_input->setClearButtonEnabled(false);

    m_send = new QPushButton(tr("Send"), this);
    m_send->setObjectName("CopilotSend");
    m_send->setEnabled(true);

    auto* composer = new QHBoxLayout;
    composer->addWidget(m_input, 1);
    composer->addWidget(m_send);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);
    layout->addLayout(header);
    layout->addWidget(m_chat, 1);
    layout->addLayout(composer);

    connect(m_send, &QPushButton::clicked, this, &CopilotPanel::sendMessage);
    connect(m_input, &QLineEdit::returnPressed, this, &CopilotPanel::sendMessage);
}

void CopilotPanel::sendMessage() {
    const QString text = m_input->text().trimmed();
    if (text.isEmpty())
        return;
    m_input->clear();
    m_chat->append(tr(
        "<div class='copilot-bubble user'><b>You</b><br>%1</div>"
        "<div class='copilot-bubble assistant'><b>Copilot</b><br>"
        "The Copilot backend isn't available in this preview build. Your "
        "request is recorded but no image was changed.</div>")
                       .arg(text.toHtmlEscaped()));
}