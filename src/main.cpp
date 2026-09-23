#include "MainWindow.h"
#include "Theme.h"

#include <QApplication>
#include <QTimer>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("WPaint"));
    QApplication::setOrganizationName(QStringLiteral("WPaint"));
    QApplication::setApplicationVersion(QStringLiteral(WP_VERSION));

    // Hidden debug flags (used by the smoke-test loop): --screenshot <file>
    // renders the window offscreen and quits; --dark forces the dark theme.
    QString shotPath;
    bool dark = false;
    for (int i = 1; i < argc - 1; ++i) {
        if (qstrcmp(argv[i], "--screenshot") == 0)
            shotPath = QString::fromLocal8Bit(argv[i + 1]);
    }
    for (int i = 1; i < argc; ++i) {
        if (qstrcmp(argv[i], "--dark") == 0)
            dark = true;
    }

    if (dark)
        Theme::setMode(Theme::Mode::Dark);

    app.setStyleSheet(Theme::stylesheet(Theme::tokens()));

    MainWindow win;
    win.show();

    if (!shotPath.isEmpty()) {
        QTimer::singleShot(900, [&] {
            const QPixmap pm = win.grab();
            pm.save(shotPath);
            QApplication::exit(0);
        });
    }

    return app.exec();
}