#include "MainWindow.h"
#include "Settings.h"
#include "Theme.h"

#include <QApplication>
#include <QIcon>
#include <QTimer>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("WPaint"));
    QApplication::setOrganizationName(QStringLiteral("WPaint"));
    QApplication::setApplicationVersion(QStringLiteral(WP_VERSION));
    // Loaded straight from the resource, not through Theme: this is the one icon
    // with colour in it, and the tinting provider would flatten it to a
    // silhouette. It is also the file the desktop entry installs, so the taskbar
    // and the file manager agree about what WPaint looks like.
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/assets/wpaint.svg")));

    // Hidden debug flags (used by the smoke-test loop): --screenshot <file>
    // renders the window offscreen and quits; --dark / --light pin the theme
    // instead of following the OS, so a smoke shot can capture both regardless
    // of the desktop's own color scheme.
    //
    // Everything that is not a flag is a file to open, which is what makes
    // "Open with WPaint" work at all: the desktop entry runs
    // `wpaint %F`, and a program that ignores its arguments cannot be a target
    // for a file manager no matter what the entry says. Several files get a
    // window each, because there are several windows now.
    QString shotPath;
    bool dark = false;
    bool light = false;
    QStringList files;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QLatin1String("--screenshot") && i + 1 < argc) {
            shotPath = QString::fromLocal8Bit(argv[i + 1]);
            ++i;
            continue;
        }
        if (arg == QLatin1String("--dark")) {
            dark = true;
            continue;
        }
        if (arg == QLatin1String("--light")) {
            light = true;
            continue;
        }
        if (arg.startsWith(QLatin1String("-")))
            continue;
        files << arg;
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
    // Shown before the files are opened, not after. A file that cannot be read
    // is reported in a modal box, and a modal box raised over a window that has
    // not been shown yet is a box over nothing -- with the smoke-test timer not
    // yet armed behind it, which is how an unreadable file turned into a hung
    // process instead of a window with an error over it.
    win.show();

    if (!shotPath.isEmpty()) {
        QTimer::singleShot(900, [&] {
            // A modal box is closed before the shot and before the exit, and
            // not as a nicety: a file that cannot be read is reported in one, it
            // runs its own event loop, and `exit()` only leaves the outermost one.
            // So smoke-testing the app with an unreadable file argument -- which
            // is what a file manager handing over a .txt does -- hung on the box
            // instead of taking the picture and quitting.
            if (QWidget* modal = QApplication::activeModalWidget())
                modal->close();
            const QPixmap pm = win.grab();
            pm.save(shotPath);
            QApplication::exit(0);
        });
    }

    // The first file goes in the window that is already there; the rest each get
    // their own, so a file manager handing over five images gives five windows
    // rather than five of them replacing one another. A file that cannot be read
    // is reported by the window that tried, and the rest still open.
    //
    // Opened from the event loop rather than before it. Reading a file can raise
    // a modal box -- it is not an image, or there is unsaved work to lose -- and a
    // modal box raised before there is a loop to run it has nowhere to draw, no
    // button to press, and an `exit()` behind it that has no loop to leave. That
    // is not a hypothetical: it is exactly what handing the app a .txt did.
    if (!files.isEmpty()) {
        QTimer::singleShot(0, [&win, files] {
            win.openFile(files.first());
            for (int i = 1; i < files.size(); ++i)
                win.openInNewWindow(files.at(i));
        });
    }

    return app.exec();
}