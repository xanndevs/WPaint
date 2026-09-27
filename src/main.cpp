#include "MainWindow.h"
#include "Settings.h"
#include "Theme.h"

#include <QApplication>
#include <QTimer>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("WPaint"));
    QApplication::setOrganizationName(QStringLiteral("WPaint"));
    QApplication::setApplicationVersion(QStringLiteral(WP_VERSION));

    // Hidden debug flags (used by the smoke-test loop): --screenshot <file>
    // renders the window offscreen and quits; --dark / --light pin the theme
    // instead of following the OS, so a smoke shot can capture both regardless
    // of the desktop's own color scheme.
    QString shotPath;
    bool dark = false;
    bool light = false;
    for (int i = 1; i < argc - 1; ++i) {
        if (qstrcmp(argv[i], "--screenshot") == 0)
            shotPath = QString::fromLocal8Bit(argv[i + 1]);
    }
    for (int i = 1; i < argc; ++i) {
        if (qstrcmp(argv[i], "--dark") == 0)
            dark = true;
        if (qstrcmp(argv[i], "--light") == 0)
            light = true;
    }

    Theme::init();
    // After the QApplication identity is set, so QSettings lands in the right
    // scope, and before any window reads a setting.
    Settings::load();
    // Before the window exists: the theme radios in the View menu are built from
    // Theme::preference(), so a stored choice has to be in place first or the
    // menu opens showing Light while the app is dark.
    Theme::setPreference(static_cast<Theme::Pref>(Settings::themePreference()));
    // Last, so the flags win. Pinning the mode before the stored preference went
    // in did nothing at all: setPreference() re-resolves the mode, and the
    // default preference is System, so --dark rendered a light window and the
    // dark screenshot that was supposed to be checked was a copy of the light
    // one. A flag that cannot disagree with the settings is not a flag.
    if (dark)
        Theme::setPreference(Theme::Pref::Dark);
    else if (light)
        Theme::setPreference(Theme::Pref::Light);

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