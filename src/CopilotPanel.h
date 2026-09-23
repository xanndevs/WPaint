#pragma once

#include <QWidget>

class QLineEdit;
class QPushButton;
class QTextBrowser;

// Stub placeholder for the Copilot rail. Charts the space, clearly signals
// that the model is not wired to this build yet.
class CopilotPanel : public QWidget {
    Q_OBJECT
public:
    explicit CopilotPanel(QWidget* parent = nullptr);

private:
    void sendMessage();

    QTextBrowser* m_chat;
    QLineEdit* m_input;
    QPushButton* m_send;
};