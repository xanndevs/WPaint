#include "SettingsDialog.h"
#include "FluentCombo.h"
#include "FluentSwitch.h"
#include "Settings.h"
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

QLabel* sectionHeader(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setObjectName("SettingsSection");
    return label;
}

} // namespace

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
        v.antialiasCanvas = m_antialias->isChecked();
        v.crispPixelsWhenMagnified = m_crispMagnified->isChecked();
        v.showBoundaryHandles = m_boundaryHandles->isChecked();
        v.spaceWheelBrushSize = m_spaceWheel->isChecked();
        v.smoothShapes = m_smoothShapes->isChecked();
        v.smoothText = m_smoothText->isChecked();
        v.confirmDiscard = m_confirmDiscard->isChecked();
        v.thumbnailQuality = m_thumbnailQuality->currentIndex();
        v.undoLimit = m_undoLimit->value();
        v.defaultShape = m_defaultShapeIds.value(m_defaultShape->currentIndex(),
                                                 static_cast<int>(ToolId::ShapeRect));
        v.defaultShapeStyle = m_defaultShapeStyle->currentIndex();
        v.defaultBrushStyle = m_defaultBrushStyle->currentIndex();
        v.defaultBrushSize = m_defaultBrushSize->value();
        v.defaultCanvasSize = m_defaultCanvasSize->currentData().toString();
        v.defaultPrimary = static_cast<ColorSwatchButton*>(m_primarySwatch)->color();
        v.defaultSecondary = static_cast<ColorSwatchButton*>(m_secondarySwatch)->color();
        v.defaultBackground = static_cast<ColorSwatchButton*>(m_backgroundSwatch)->color();
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

FluentSwitch* SettingsDialog::addSwitch(const QString& title, const QString& description,
                                        bool checked, QWidget* page, QVBoxLayout* body) {
    auto* row = new SettingsRow(title, description, page);
    auto* sw = new FluentSwitch(row);
    sw->setChecked(checked);
    row->setControl(sw);
    body->addWidget(row);
    return sw;
}

QComboBox* SettingsDialog::addCombo(const QString& title, const QString& description,
                                    const QStringList& items, int index, QWidget* page,
                                    QVBoxLayout* body) {
    auto* row = new SettingsRow(title, description, page);
    auto* combo = new FluentCombo(row);
    // The item text is a sentence, and a combo sized to its widest item would
    // stretch the row to the full page width.
    combo->setMaximumWidth(260);
    combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    combo->addItems(items);
    combo->setCurrentIndex(qBound(0, index, items.size() - 1));
    row->setControl(combo);
    body->addWidget(row);
    return combo;
}

void SettingsDialog::buildBehaviorPage() {
    m_behaviorPage = makePage(tr("Behavior"),
                              tr("How the editor responds while you work. None of "
                                 "these change what gets saved."),
                              &m_behaviorBody);
    auto* w = m_behaviorPage;
    auto* body = m_behaviorBody;
    const SettingsValues& v = Settings::values();

    body->addWidget(sectionHeader(tr("Canvas"), w));
    m_antialias = addSwitch(
        tr("Antialias the canvas"),
        tr("Blend pixels as the document is scaled, so magnified edges stay "
           "smooth instead of blocky. Shapes and text keep their own smooth "
           "edges either way, and saved files are never affected."),
        v.antialiasCanvas, w, body);
    m_crispMagnified = addSwitch(
        tr("Fall back to hard pixels above 100%"),
        tr("Once you have magnified past actual size, show every document pixel "
           "as a solid block. Useful when you are editing pixel art, and it "
           "overrides the antialiasing above while it is on."),
        v.crispPixelsWhenMagnified, w, body);
    m_boundaryHandles = addSwitch(
        tr("Show resize handles on the canvas edge"),
        tr("Draw the eight grab handles just outside the document, so the canvas "
           "can be dragged to a new size. Turning this off hides them; it does "
           "not change how a resize behaves when you start one."),
        v.showBoundaryHandles, w, body);

    body->addWidget(sectionHeader(tr("Tools"), w));
    m_smoothShapes = addSwitch(
        tr("Smooth the outlines of shapes you draw"),
        tr("Blend the edge of a rectangle, line or arrow into the pixels around "
           "it. Turn it off to keep every shape perfectly hard-edged."),
        v.smoothShapes, w, body);
    m_smoothText = addSwitch(
        tr("Smooth the edges of text you place"),
        tr("Blend text into the canvas as it is drawn. Turning it off is the "
           "same choice pixel artists make for type."),
        v.smoothText, w, body);
    m_spaceWheel = addSwitch(
        tr("Let Space and the mouse wheel change the brush size"),
        tr("Hold Space and scroll to make the brush bigger or smaller without "
           "leaving the canvas. With this off, Space and the wheel scroll the "
           "document as usual."),
        v.spaceWheelBrushSize, w, body);
    m_thumbnailQuality = addCombo(
        tr("Scale the layer thumbnails smoothly"),
        tr("Fast keeps each thumbnail pixel a hard block, which is what you want "
           "when you are judging pixel art. Smooth blends them instead."),
        {tr("Fast — keep the pixels square"), tr("Smooth — blend the pixels")},
        v.thumbnailQuality, w, body);

    body->addWidget(sectionHeader(tr("History and files"), w));
    auto* undoRow = new SettingsRow(
        tr("Limit how many steps you can undo"),
        tr("Once the history is full, the oldest step is dropped. Zero keeps "
           "everything, which is fine for an image of a reasonable size."),
        w);
    m_undoLimit = new QSpinBox(undoRow);
    m_undoLimit->setRange(0, 10000);
    m_undoLimit->setSingleStep(10);
    m_undoLimit->setSpecialValueText(tr("Unlimited"));
    m_undoLimit->setValue(v.undoLimit);
    undoRow->setControl(m_undoLimit);
    body->addWidget(undoRow);
    m_confirmDiscard = addSwitch(
        tr("Ask before discarding unsaved changes"),
        tr("When you open, close or start a new image with edits you have not "
           "saved, WPaint offers to save them first. Turning this off discards "
           "them silently."),
        v.confirmDiscard, w, body);
    body->addStretch(1);

    m_pages->addWidget(w);
    m_nav->addItem(tr("Behavior"));
}

void SettingsDialog::buildDefaultsPage() {
    m_defaultsPage = makePage(
        tr("Defaults"),
        tr("What a new image starts as. Changing any of these does not touch the "
           "document you have open."),
        &m_defaultsBody);
    auto* w = m_defaultsPage;
    auto* body = m_defaultsBody;
    const SettingsValues& v = Settings::values();

    body->addWidget(sectionHeader(tr("The shape tool"), w));
    QStringList shapeNames;
    QList<int> shapeIds;
    for (const auto& s : ToolRegistry::specs()) {
        if (!s.inShapes) continue;
        shapeNames << s.name;
        shapeIds << static_cast<int>(s.id);
    }
    // Stored as the ToolId, not the index: the gallery order is a presentation
    // choice and reordering it must not silently repoint somebody's default.
    m_defaultShape = addCombo(tr("Start with this shape"),
                              tr("The shape the gallery opens on when a new image "
                                 "is created."),
                              shapeNames, shapeIds.indexOf(v.defaultShape), w, body);
    m_defaultShapeIds = shapeIds;
    QStringList styleNames;
    for (ShapeStyle st : kShapeStyles)
        styleNames << shapeStyleName(st);
    m_defaultShapeStyle = addCombo(tr("Start with this fill mode"),
                                   tr("Outline draws the edge only, Fill fills "
                                      "the shape, and Outline and fill does both."),
                                   styleNames, v.defaultShapeStyle, w, body);

    body->addWidget(sectionHeader(tr("The brush"), w));
    QStringList brushNames;
    for (BrushStyle bs : kBrushStyles)
        brushNames << brushStyleName(bs);
    m_defaultBrushStyle = addCombo(tr("Start with this brush"),
                                   tr("The brush style a new image begins with."),
                                   brushNames, v.defaultBrushStyle, w, body);
    auto* sizeRow = new SettingsRow(
        tr("Start with this brush size"),
        tr("The thickness of the stroke the first time you draw in a new image."),
        w);
    m_defaultBrushSize = new QSpinBox(sizeRow);
    m_defaultBrushSize->setRange(1, kMaxBrushSize);
    m_defaultBrushSize->setValue(v.defaultBrushSize);
    sizeRow->setControl(m_defaultBrushSize);
    body->addWidget(sizeRow);

    body->addWidget(sectionHeader(tr("Colors"), w));
    auto makeSwatch = [&](const QColor& c, const QString& title, QWidget* parent) {
        auto* swatch = new ColorSwatchButton(c, title, parent);
        connect(swatch, &ColorSwatchButton::picked, swatch, [swatch, title] {
            const QColor chosen = QColorDialog::getColor(swatch->color(), swatch, title);
            if (chosen.isValid())
                swatch->setColor(chosen);
        });
        return swatch;
    };
    auto* primaryRow = new SettingsRow(
        tr("Start with these two colors"),
        tr("Color 1 is what you draw with; color 2 is what right-click draws "
           "with, and the pair swaps when you press X."),
        w);
    m_primarySwatch = makeSwatch(v.defaultPrimary, tr("Color 1"), primaryRow);
    m_secondarySwatch = makeSwatch(v.defaultSecondary, tr("Color 2"), primaryRow);
    primaryRow->controlLayout()->addWidget(m_primarySwatch);
    primaryRow->controlLayout()->addWidget(m_secondarySwatch);
    body->addWidget(primaryRow);
    auto* backgroundRow = new SettingsRow(
        tr("Start with this background"),
        tr("The bottom layer of a new image. Transparent is also available from "
           "the colour editor."),
        w);
    m_backgroundSwatch = makeSwatch(v.defaultBackground, tr("Background"), backgroundRow);
    backgroundRow->setControl(m_backgroundSwatch);
    body->addWidget(backgroundRow);

    body->addWidget(sectionHeader(tr("The document"), w));
    m_defaultCanvasSize = addCombo(
        tr("Start new images at this size"),
        tr("The width and height of a new image. The size can still be changed "
           "from the Resize and rotate dialog afterwards."),
        {tr("400 × 400 (square)"), tr("800 × 600"), tr("1280 × 720"),
         tr("1920 × 1080"), tr("Keep the last size used")},
        0, w, body);
    // The list order is the display order, not the stored value: the value is
    // the "W x H" string, or empty to mean "keep whatever was last used".
    static const QStringList kSizes = {"400x400", "800x600", "1280x720", "1920x1080",
                                       QString()};
    m_defaultCanvasSize->setCurrentIndex(qMax(0, kSizes.indexOf(v.defaultCanvasSize)));
    body->addStretch(1);

    m_pages->addWidget(w);
    m_nav->addItem(tr("Defaults"));
}

void SettingsDialog::buildAboutPage() {
    QVBoxLayout* body = nullptr;
    QWidget* w = makePage(tr("About"), QString(), &body);
    auto* blurb = new QLabel(
        tr("WPaint is a layered image editor that borrows its layout from "
           "Windows 11 Paint: grouped toolbars, a layers rail and a size "
           "strip, with the document as a stack of images rather than a "
           "single flattened bitmap."),
        w);
    blurb->setObjectName("SettingsAboutBlurb");
    blurb->setWordWrap(true);
    body->addWidget(blurb);

    body->addWidget(sectionHeader(tr("Version"), w));
    for (const QString& line : {tr("WPaint %1").arg(QLatin1String(WP_VERSION)),
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
    m_antialias->setChecked(Settings::antialiasCanvas());
    m_crispMagnified->setChecked(Settings::crispPixelsWhenMagnified());
    m_boundaryHandles->setChecked(Settings::showBoundaryHandles());
    m_spaceWheel->setChecked(Settings::spaceWheelBrushSize());
    m_smoothShapes->setChecked(Settings::smoothShapes());
    m_smoothText->setChecked(Settings::smoothText());
    m_confirmDiscard->setChecked(Settings::confirmDiscard());
    m_thumbnailQuality->setCurrentIndex(Settings::thumbnailQuality());
    m_undoLimit->setValue(Settings::undoLimit());
    m_defaultShape->setCurrentIndex(Settings::defaultShape());
    m_defaultShapeStyle->setCurrentIndex(Settings::defaultShapeStyle());
    m_defaultBrushStyle->setCurrentIndex(Settings::defaultBrushStyle());
    m_defaultBrushSize->setValue(Settings::defaultBrushSize());
    static_cast<ColorSwatchButton*>(m_primarySwatch)->setColor(Settings::defaultPrimary());
    static_cast<ColorSwatchButton*>(m_secondarySwatch)->setColor(Settings::defaultSecondary());
    static_cast<ColorSwatchButton*>(m_backgroundSwatch)->setColor(Settings::defaultBackground());
}
