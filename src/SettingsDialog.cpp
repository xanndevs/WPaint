#include "SettingsDialog.h"
#include "ColorDialog.h"
#include "FluentCombo.h"
#include "FluentSwitch.h"
#include "PalettePresets.h"
#include "Settings.h"
#include "SettingsPage.h"
#include "SettingsRow.h"
#include "SettingsSwatchButton.h"
#include "Theme.h"
#include "Tool.h"

#include <QAbstractButton>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

const QList<BrushStyle> kBrushStyles = {BrushStyle::Round, BrushStyle::Square,
                                        BrushStyle::Spray, BrushStyle::Calligraphy};

const QList<ShapeStyle> kShapeStyles = {ShapeStyle::Outline, ShapeStyle::Fill,
                                        ShapeStyle::OutlineFill};

QString brushStyleName(BrushStyle s) {
    switch (s) {
    case BrushStyle::Round: return QObject::tr("Round");
    case BrushStyle::Square: return QObject::tr("Square");
    case BrushStyle::Spray: return QObject::tr("Spray");
    case BrushStyle::Calligraphy: return QObject::tr("Calligraphy");
    }
    return QString();
}

QString shapeStyleName(ShapeStyle s) {
    switch (s) {
    case ShapeStyle::Outline: return QObject::tr("Outline");
    case ShapeStyle::Fill: return QObject::tr("Fill");
    case ShapeStyle::OutlineFill: return QObject::tr("Outline and fill");
    }
    return QString();
}

// A page is a title, a one-line explanation of what the section is for, and then
// as much scrolling content as it needs. Only the content scrolls, so the
// section heading stays put while a long page is scrolled.
QWidget* makePage(const QString& title, const QString& subtitle, QVBoxLayout** outBody) {
    const auto& t = Theme::tokens();
    auto* page = new QWidget;
    page->setObjectName("SettingsPage");
    auto* root = new QVBoxLayout(page);
    // Breathing room on the left of the text column, so the page heading does
    // not sit flush against the rail it belongs to.
    root->setContentsMargins(t.pad * 3, t.pad, 0, 0);
    root->setSpacing(2);

    auto* heading = new QLabel(title, page);
    heading->setObjectName("SettingsPageTitle");
    root->addWidget(heading);

    if (!subtitle.isEmpty()) {
        auto* sub = new QLabel(subtitle, page);
        sub->setObjectName("SettingsPageSub");
        sub->setWordWrap(true);
        root->addWidget(sub);
    }

    auto* scroll = new QScrollArea(page);
    scroll->setObjectName("SettingsScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* body = new QWidget;
    body->setObjectName("SettingsPage");
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(0, 4, t.pad, t.pad * 2);
    bodyLayout->setSpacing(2);
    scroll->setWidget(body);
    root->addWidget(scroll, 1);

    *outBody = bodyLayout;
    return page;
}

// The stored value for the new-canvas-size row. The dropdown shows a sentence and
// saves the pair, so the two lists are index-aligned: the list order is the
// display order, not the value.
const QStringList kCanvasSizes = {"400x400", "800x600", "1280x720", "1920x1080", QString()};

QLabel* sectionHeader(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setObjectName("SettingsSection");
    return label;
}

} // namespace

using Desc = SettingsDialog::Description;

// A row whose control is a FluentSwitch. `out` is handed back so the caller can
// seed it from the table without reaching into the widget.
QWidget* makeSwitch(SettingsRow* row) { return new FluentSwitch(row); }

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Preferences"));
    setModal(true);
    resize(880, 600);
    setMinimumSize(680, 480);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* header = new QWidget(this);
    auto* headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(Theme::tokens().pad * 2, Theme::tokens().pad * 2,
                                     Theme::tokens().pad * 2, Theme::tokens().pad);
    headerLayout->setSpacing(2);
    auto* title = new QLabel(tr("Preferences"), header);
    title->setObjectName("DialogHeader");
    auto* subtitle = new QLabel(
        tr("WPaint %1 — how the editor behaves, what a new document starts as, "
           "and what every key does.")
            .arg(QLatin1String(WP_VERSION)),
        header);
    subtitle->setObjectName("PreferencesHint");
    headerLayout->addWidget(title);
    headerLayout->addWidget(subtitle);
    root->addWidget(header);

    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);

    m_nav = new QListWidget(this);
    m_nav->setObjectName("SettingsNav");
    m_nav->setFixedWidth(Theme::tokens().settingsNavW);
    m_nav->setFrameShape(QFrame::NoFrame);
    m_nav->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    body->addWidget(m_nav);

    m_pages = new QStackedWidget(this);
    body->addWidget(m_pages, 1);
    root->addLayout(body, 1);

    buildBehaviorPage();
    buildDefaultsPage();
    buildShortcutsPage();
    buildAboutPage();

    connect(m_nav, &QListWidget::currentRowChanged, m_pages, &QStackedWidget::setCurrentIndex);
    m_nav->setCurrentRow(0);

    auto* hint = new QLabel(
        tr("Changes take effect as soon as you press OK. Cancel leaves everything "
           "as it was."),
        this);
    hint->setObjectName("PreferencesHint");
    hint->setContentsMargins(Theme::tokens().pad * 2, 0, 0, 0);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults,
        this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        SettingsValues v = Settings::values();
        for (auto it = m_writers.constBegin(); it != m_writers.constEnd(); ++it) {
            auto c = m_controls.constFind(it.key());
            if (c != m_controls.constEnd())
                it.value()(v, *c);
        }
        Settings::apply(v);
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
    connect(buttons, &QDialogButtonBox::clicked, this, [this, buttons](QAbstractButton* b) {
        if (buttons->buttonRole(b) != QDialogButtonBox::ResetRole)
            return;
        Settings::resetDefaults();
        reload();
    });

    auto* footer = new QHBoxLayout;
    footer->setContentsMargins(0, 0, Theme::tokens().pad * 2, Theme::tokens().pad * 2);
    footer->addWidget(hint, 1);
    footer->addWidget(buttons);
    root->addLayout(footer);

    reload();
}

void SettingsDialog::buildPage(const QList<RowSpec>& specs, const QString& title,
                              const QString& subtitle) {
    QVBoxLayout* body = nullptr;
    QWidget* page = makePage(title, subtitle, &body);
    QString section;
    for (const RowSpec& spec : specs) {
        if (QString::fromLatin1(spec.section) != section) {
            section = QString::fromLatin1(spec.section);
            body->addWidget(sectionHeader(section, page));
        }
        auto* row = new SettingsRow(tr(spec.title), tr(spec.description), page, spec.desc);
        QWidget* control = spec.make(row);
        row->setControl(control);
        m_controls.insert(QLatin1String(spec.id), control);
        m_writers.insert(QLatin1String(spec.id), spec.write);
        body->addWidget(row);
    }
    body->addStretch(1);
    m_pages->addWidget(page);
    m_nav->addItem(title);
}

void SettingsDialog::buildBehaviorPage() {
    const SettingsValues& v = Settings::values();
    const QList<RowSpec> specs = {
        {"Canvas", "antialias", "Antialias the canvas",
         "Blend pixels as the document is scaled, so magnified edges stay smooth "
         "instead of blocky. Shapes and text keep their own smooth edges either "
         "way, and saved files are never affected.",
         Desc::Tooltip,
         [](SettingsRow* r) { return makeSwitch(r); },
         [](SettingsValues& out, QWidget* w) {
             out.antialiasCanvas = static_cast<FluentSwitch*>(w)->isChecked();
         }},
        {"Canvas", "crispAbove100", "Fall back to hard pixels above 100%",
         "Once you have magnified past actual size, show every document pixel as a "
         "solid block. Useful when you are editing pixel art, and it overrides the "
         "antialiasing above while it is on.",
         Desc::Tooltip,
         [](SettingsRow* r) { return makeSwitch(r); },
         [](SettingsValues& out, QWidget* w) {
             out.crispPixelsWhenMagnified = static_cast<FluentSwitch*>(w)->isChecked();
         }},
        {"Canvas", "boundaryHandles", "Show resize handles on the canvas edge",
         "Draw the eight grab handles just outside the document, so the canvas can "
         "be dragged to a new size. Turning this off hides them; it does not change "
         "how a resize behaves once you start one.",
         Desc::Tooltip,
         [](SettingsRow* r) { return makeSwitch(r); },
         [](SettingsValues& out, QWidget* w) {
             out.showBoundaryHandles = static_cast<FluentSwitch*>(w)->isChecked();
         }},
        {"Tools", "smoothShapes", "Smooth the outlines of shapes you draw",
         "Blend the edge of a rectangle, line or arrow into the pixels around it. "
         "Turn it off to keep every shape perfectly hard-edged.",
         Desc::Tooltip,
         [](SettingsRow* r) { return makeSwitch(r); },
         [](SettingsValues& out, QWidget* w) {
             out.smoothShapes = static_cast<FluentSwitch*>(w)->isChecked();
         }},
        {"Tools", "smoothText", "Smooth the edges of text you place",
         "Blend text into the canvas as it is drawn. Turning it off is the same "
         "choice pixel artists make for type.",
         Desc::Tooltip,
         [](SettingsRow* r) { return makeSwitch(r); },
         [](SettingsValues& out, QWidget* w) {
             out.smoothText = static_cast<FluentSwitch*>(w)->isChecked();
         }},
        {"Tools", "spaceWheel", "Let Space and the mouse wheel change the brush size",
         "Hold Space and scroll to make the brush bigger or smaller without leaving "
         "the canvas. With this off, Space and the wheel scroll the document as "
         "usual.",
         Desc::Tooltip,
         [](SettingsRow* r) { return makeSwitch(r); },
         [](SettingsValues& out, QWidget* w) {
             out.spaceWheelBrushSize = static_cast<FluentSwitch*>(w)->isChecked();
         }},
        {"Tools", "thumbnailQuality", "Scale the layer thumbnails smoothly",
         "Fast keeps each thumbnail pixel a hard block, which is what you want when "
         "you are judging pixel art. Smooth blends them instead.",
         Desc::Tooltip,
         [](SettingsRow* r) {
             auto* combo = new FluentCombo(r);
             // The item text is a sentence, and a combo sized to its widest item
             // would stretch the row across the page.
             combo->setMaximumWidth(260);
             combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
             combo->addItems({QObject::tr("Fast — keep the pixels square"),
                              QObject::tr("Smooth — blend the pixels")});
             return combo;
         },
         [](SettingsValues& out, QWidget* w) {
             out.thumbnailQuality = static_cast<QComboBox*>(w)->currentIndex();
         }},
        {"History and files", "undoLimit", "Limit how many steps you can undo",
         "Once the history is full, the oldest step is dropped. Zero keeps "
         "everything, which is fine for an image of a reasonable size.",
         Desc::Tooltip,
         [](SettingsRow* r) {
             auto* spin = new QSpinBox(r);
             spin->setRange(0, 10000);
             spin->setSingleStep(10);
             spin->setSpecialValueText(QObject::tr("Unlimited"));
             return spin;
         },
         [](SettingsValues& out, QWidget* w) {
             out.undoLimit = static_cast<QSpinBox*>(w)->value();
         }},
        {"History and files", "confirmDiscard",
         "Ask before discarding unsaved changes",
         "When you open, close or start a new image with edits you have not saved, "
         "WPaint offers to save them first. Turning this off discards them silently.",
         Desc::Tooltip,
         [](SettingsRow* r) { return makeSwitch(r); },
         [](SettingsValues& out, QWidget* w) {
             out.confirmDiscard = static_cast<FluentSwitch*>(w)->isChecked();
         }},
    };
    buildPage(specs, tr("Behavior"),
              tr("How the editor responds while you work. None of these change what "
                 "gets saved."));
    // Seeded after the page is built so the wiring above stays a list of prose.
    static_cast<FluentSwitch*>(m_controls.value("antialias"))->setChecked(v.antialiasCanvas);
    static_cast<FluentSwitch*>(m_controls.value("crispAbove100"))
        ->setChecked(v.crispPixelsWhenMagnified);
    static_cast<FluentSwitch*>(m_controls.value("boundaryHandles"))
        ->setChecked(v.showBoundaryHandles);
    static_cast<FluentSwitch*>(m_controls.value("smoothShapes"))->setChecked(v.smoothShapes);
    static_cast<FluentSwitch*>(m_controls.value("smoothText"))->setChecked(v.smoothText);
    static_cast<FluentSwitch*>(m_controls.value("spaceWheel"))
        ->setChecked(v.spaceWheelBrushSize);
    static_cast<FluentSwitch*>(m_controls.value("confirmDiscard"))
        ->setChecked(v.confirmDiscard);
    static_cast<QComboBox*>(m_controls.value("thumbnailQuality"))
        ->setCurrentIndex(v.thumbnailQuality);
    static_cast<QSpinBox*>(m_controls.value("undoLimit"))->setValue(v.undoLimit);
}

void SettingsDialog::buildDefaultsPage() {
    const SettingsValues& v = Settings::values();
    QStringList shapeNames;
    QList<int> shapeIds;
    for (const auto& s : ToolRegistry::specs()) {
        if (!s.inShapes) continue;
        shapeNames << s.name;
        shapeIds << static_cast<int>(s.id);
    }
    QStringList styleNames;
    for (ShapeStyle st : kShapeStyles)
        styleNames << shapeStyleName(st);
    QStringList brushNames;
    for (BrushStyle bs : kBrushStyles)
        brushNames << brushStyleName(bs);
    QStringList sizeNames = {tr("400 × 400 (square)"), tr("800 × 600"), tr("1280 × 720"),
                             tr("1920 × 1080"), tr("Keep the last size used")};
    QStringList paletteNames;
    QVector<QVector<QColor>> paletteSwatches;
    for (const auto& p : PalettePresets::all()) {
        paletteNames << p.name;
        paletteSwatches << p.colors;
    }
    paletteSwatches[PalettePresets::customIndex()] = Settings::paletteCustom();

    // Two swatches count as one control, so the row reads as one sentence.
    auto makeColorPair = [this](SettingsRow* row, const QString& tooltip) {
        auto* holder = new QWidget(row);
        auto* line = new QHBoxLayout(holder);
        line->setContentsMargins(0, 0, 0, 0);
        line->setSpacing(Theme::tokens().gap);
        for (const QString& which : {tr("Color 1"), tr("Color 2")}) {
            auto* swatch = new ColorSwatchButton(Qt::white, which, holder);
            swatch->setToolTip(tooltip);
            connect(swatch, &ColorSwatchButton::picked, swatch, [swatch, which] {
                const QColor chosen = QColorDialog::getColor(swatch->color(), swatch, which);
                if (chosen.isValid())
                    swatch->setColor(chosen);
            });
            line->addWidget(swatch);
        }
        return holder;
    };
    auto makeSingleSwatch = [this](SettingsRow* row, const QString& tooltip) {
        auto* swatch = new ColorSwatchButton(Qt::white, tooltip, row);
        swatch->setToolTip(tooltip);
        connect(swatch, &ColorSwatchButton::picked, swatch, [swatch, tooltip] {
            const QColor chosen = QColorDialog::getColor(swatch->color(), swatch, tooltip);
            if (chosen.isValid())
                swatch->setColor(chosen);
        });
        return swatch;
    };
    // A row's control is either a swatch or a box of them, and the writer should
    // not have to care which.
    auto swatchOf = [](QWidget* host, int index) -> ColorSwatchButton* {
        if (auto* one = qobject_cast<ColorSwatchButton*>(host))
            return index == 0 ? one : nullptr;
        return host->findChildren<ColorSwatchButton*>().value(index);
    };

    const QList<RowSpec> specs = {
        {"The shape tool", "shape", "Start with this shape",
         "The shape the gallery opens on when a new image is created. Stored as the "
         "shape itself rather than its position, so reordering the gallery cannot "
         "silently repoint it.",
         Desc::Tooltip,
         [shapeNames](SettingsRow* r) {
             auto* combo = new FluentCombo(r);
             combo->setMaximumWidth(260);
             combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
             combo->addItems(shapeNames);
             return combo;
         },
         [shapeIds](SettingsValues& out, QWidget* w) {
             out.defaultShape = shapeIds.value(static_cast<QComboBox*>(w)->currentIndex(),
                                               static_cast<int>(ToolId::ShapeRect));
         }},
        {"The shape tool", "shapeStyle", "Start with this fill mode",
         "Outline draws the edge only, Fill fills the shape, and Outline and fill "
         "does both.",
         Desc::Tooltip,
         [styleNames](SettingsRow* r) {
             auto* combo = new FluentCombo(r);
             combo->setMaximumWidth(260);
             combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
             combo->addItems(styleNames);
             return combo;
         },
         [](SettingsValues& out, QWidget* w) {
             out.defaultShapeStyle = static_cast<QComboBox*>(w)->currentIndex();
         }},
        {"The brush", "brushStyle", "Start with this brush",
         "The brush style a new image begins with.",
         Desc::Tooltip,
         [brushNames](SettingsRow* r) {
             auto* combo = new FluentCombo(r);
             combo->setMaximumWidth(260);
             combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
             combo->addItems(brushNames);
             return combo;
         },
         [](SettingsValues& out, QWidget* w) {
             out.defaultBrushStyle = static_cast<QComboBox*>(w)->currentIndex();
         }},
        {"The brush", "brushSize", "Start with this brush size",
         "The thickness of the stroke the first time you draw in a new image.",
         Desc::Tooltip,
         [](SettingsRow* r) {
             auto* spin = new QSpinBox(r);
             spin->setRange(1, kMaxBrushSize);
             return spin;
         },
         [](SettingsValues& out, QWidget* w) {
             out.defaultBrushSize = static_cast<QSpinBox*>(w)->value();
         }},
        {"Colors", "colorPair", "Start with these two colors",
         "Color 1 is what you draw with, color 2 is what right-click draws with, "
         "and the pair swaps when you press X.",
         Desc::Tooltip,
         [makeColorPair](SettingsRow* r) {
             return makeColorPair(r, r->toolTip().isEmpty() ? QString() : r->toolTip());
         },
         [swatchOf](SettingsValues& out, QWidget* w) {
             if (auto* a = swatchOf(w, 0)) out.defaultPrimary = a->color();
             if (auto* b = swatchOf(w, 1)) out.defaultSecondary = b->color();
         }},
        {"Colors", "background", "Start with this background",
         "The bottom layer of a new image. Transparent is available from the colour "
         "editor.",
         Desc::Inline,
         [makeSingleSwatch](SettingsRow* r) {
             return makeSingleSwatch(r, r->toolTip().isEmpty() ? QString() : r->toolTip());
         },
         [swatchOf](SettingsValues& out, QWidget* w) {
             if (auto* swatch = swatchOf(w, 0)) out.defaultBackground = swatch->color();
         }},
        {"Colors", "palette", "The colors in the toolbar",
         "Twenty swatches, drawn as the toolbar draws them: the first ten on the top "
         "row, the next ten below. Pick Custom to build your own set, and the editor "
         "appears under this row.",
         Desc::Inline,
         [paletteNames, paletteSwatches](SettingsRow* r) {
             auto* combo = new PalettePresetCombo(r);
             combo->setMaximumWidth(360);
             combo->setPresets(paletteNames, paletteSwatches);
             auto* editor = new PaletteEditor(r);
             editor->hide();
             // The editor only exists once Custom is picked: there is nothing to
             // edit otherwise, and a row of twenty blanks on every visit would be
             // noise.
             connect(combo, &QComboBox::currentIndexChanged, editor,
                     [editor](int at) {
                         editor->setVisible(at == PalettePresets::customIndex());
                     });
             combo->setCurrentIndex(PalettePresets::customIndex());
             editor->setVisible(combo->currentIndex() == PalettePresets::customIndex());
             r->addContent(editor);
             return combo;
         },
         [](SettingsValues& out, QWidget* w) {
             out.palettePreset = static_cast<PalettePresetCombo*>(w)->selected();
             if (auto* editor = w->parentWidget()->findChild<PaletteEditor*>())
                 out.paletteCustom = editor->colors();
         }},
        {"The document", "canvasSize", "Start new images at this size",
         "The width and height of a new image. The size can still be changed from the "
         "Resize and rotate dialog afterwards.",
         Desc::Tooltip,
         [sizeNames](SettingsRow* r) {
             auto* combo = new FluentCombo(r);
             combo->setMaximumWidth(260);
             combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
             combo->addItems(sizeNames);
             return combo;
         },
         [](SettingsValues& out, QWidget* w) {
             out.defaultCanvasSize =
                 kCanvasSizes.value(static_cast<QComboBox*>(w)->currentIndex());
         }},
    };
    buildPage(specs, tr("Defaults"),
              tr("What a new image starts as. Changing any of these does not touch the "
                 "document you have open."));

    // The stored shape is a ToolId and the combo's index is a gallery position;
    // they are not the same number.
    static_cast<QComboBox*>(m_controls.value("shape"))
        ->setCurrentIndex(shapeIds.indexOf(v.defaultShape));
    static_cast<QComboBox*>(m_controls.value("shapeStyle"))
        ->setCurrentIndex(v.defaultShapeStyle);
    static_cast<QComboBox*>(m_controls.value("brushStyle"))
        ->setCurrentIndex(v.defaultBrushStyle);
    static_cast<QSpinBox*>(m_controls.value("brushSize"))->setValue(v.defaultBrushSize);
    static_cast<QComboBox*>(m_controls.value("canvasSize"))
        ->setCurrentIndex(qMax(0, kCanvasSizes.indexOf(v.defaultCanvasSize)));
    static_cast<PalettePresetCombo*>(m_controls.value("palette"))
        ->setSelected(v.palettePreset);
    // Swatches and the editor live under their row, so they are found rather
    // than held: the row owns them, and holding a second pointer to a widget's
    // child is how a stale one survives a page rebuild.
    if (auto* a = swatchOf(m_controls.value("colorPair"), 0))
        a->setColor(v.defaultPrimary);
    if (auto* b = swatchOf(m_controls.value("colorPair"), 1))
        b->setColor(v.defaultSecondary);
    if (auto* bg = swatchOf(m_controls.value("background"), 0))
        bg->setColor(v.defaultBackground);
    if (auto* paletteRow = m_controls.value("palette")) {
        if (auto* editor = paletteRow->parentWidget()->findChild<PaletteEditor*>()) {
            editor->setColors(v.paletteCustom);
            editor->setVisible(static_cast<PalettePresetCombo*>(paletteRow)->selected() ==
                               PalettePresets::customIndex());
        }
    }
}

void SettingsDialog::buildShortcutsPage() {
    m_shortcuts = new SettingsPage;
    m_pages->addWidget(m_shortcuts);
    m_nav->addItem(tr("Shortcuts"));
}

void SettingsDialog::buildAboutPage() {
    QVBoxLayout* body = nullptr;
    QWidget* w = makePage(tr("About"), QString(), &body);
    auto* blurb = new QLabel(
        tr("WPaint is a layered image editor that borrows its layout from Windows 11 "
           "Paint: grouped toolbars, a layers rail and a size strip, with the document "
           "as a stack of images rather than a single flattened bitmap."),
        w);
    blurb->setObjectName("SettingsAboutBlurb");
    blurb->setWordWrap(true);
    body->addWidget(blurb);

    body->addWidget(sectionHeader(tr("Version"), w));
    for (const QString& line :
         {tr("WPaint %1").arg(QLatin1String(WP_VERSION)),
          tr("Built with Qt %1").arg(QLatin1String(qVersion()))}) {
        auto* value = new QLabel(line, w);
        value->setObjectName("SettingsKeyValue");
        body->addWidget(value);
    }
    body->addStretch(1);
    m_pages->addWidget(w);
    m_nav->addItem(tr("About"));
}

void SettingsDialog::reload() {
    SettingsValues v = Settings::values();
    auto sw = [this](const char* id) {
        return static_cast<FluentSwitch*>(m_controls.value(QLatin1String(id)));
    };
    auto combo = [this](const char* id) {
        return static_cast<QComboBox*>(m_controls.value(QLatin1String(id)));
    };
    auto spin = [this](const char* id) {
        return static_cast<QSpinBox*>(m_controls.value(QLatin1String(id)));
    };
    sw("antialias")->setChecked(v.antialiasCanvas);
    sw("crispAbove100")->setChecked(v.crispPixelsWhenMagnified);
    sw("boundaryHandles")->setChecked(v.showBoundaryHandles);
    sw("smoothShapes")->setChecked(v.smoothShapes);
    sw("smoothText")->setChecked(v.smoothText);
    sw("spaceWheel")->setChecked(v.spaceWheelBrushSize);
    sw("confirmDiscard")->setChecked(v.confirmDiscard);
    combo("thumbnailQuality")->setCurrentIndex(v.thumbnailQuality);
    spin("undoLimit")->setValue(v.undoLimit);
    combo("shapeStyle")->setCurrentIndex(v.defaultShapeStyle);
    combo("brushStyle")->setCurrentIndex(v.defaultBrushStyle);
    spin("brushSize")->setValue(v.defaultBrushSize);
    combo("canvasSize")->setCurrentIndex(qMax(0, kCanvasSizes.indexOf(v.defaultCanvasSize)));
    m_shortcuts->reload();
}
