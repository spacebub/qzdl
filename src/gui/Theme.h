/*
 * This file is part of qZDL
 * Copyright (C) 2026  spacebub
 *
 * qZDL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <QColor>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QObject>
#include <QStyleHints>
// ReSharper disable once CppUnusedIncludeDirective
#include <QtQml/qqmlregistration.h> // Has to be here for the qml compiler

#include "core/Session.h"

/*
One palette, in two shades, and one ladder of sizes. Everything the interface
paints and every size it sets comes from here, so the whole application
changes with a single value.

The palette is the mark's own: the tile the icon is drawn on, the brushed steel
of the numeral, and the ember the wordmark is ramped in.
*/
class Theme : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    /*
    Which shade is painted, and why. The mode is what the user chose and is
    kept in the config file; dark is what that works out to right now, which
    for "system" is whatever the desktop is set to at the moment.
    */
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY changed)
    Q_PROPERTY(bool dark READ dark NOTIFY changed)

    Q_PROPERTY(QColor background READ background NOTIFY changed)
    Q_PROPERTY(QColor surface READ surface NOTIFY changed)
    Q_PROPERTY(QColor raised READ raised NOTIFY changed)
    Q_PROPERTY(QColor sunken READ sunken NOTIFY changed)
    Q_PROPERTY(QColor field READ field NOTIFY changed)
    Q_PROPERTY(QColor hover READ hover NOTIFY changed)
    Q_PROPERTY(QColor border READ border NOTIFY changed)
    Q_PROPERTY(QColor borderStrong READ borderStrong NOTIFY changed)
    Q_PROPERTY(QColor text READ text NOTIFY changed)
    Q_PROPERTY(QColor muted READ muted NOTIFY changed)
    Q_PROPERTY(QColor faint READ faint NOTIFY changed)
    Q_PROPERTY(QColor accent READ accent NOTIFY changed)
    Q_PROPERTY(QColor accentHover READ accentHover NOTIFY changed)
    Q_PROPERTY(QColor accentText READ accentText NOTIFY changed)
    Q_PROPERTY(QColor accentSoft READ accentSoft NOTIFY changed)
    Q_PROPERTY(QColor mutedSoft READ mutedSoft NOTIFY changed)
    Q_PROPERTY(QColor success READ success NOTIFY changed)
    Q_PROPERTY(QColor successSoft READ successSoft NOTIFY changed)
    Q_PROPERTY(QColor warning READ warning NOTIFY changed)
    Q_PROPERTY(QColor warningSoft READ warningSoft NOTIFY changed)
    Q_PROPERTY(QColor danger READ danger NOTIFY changed)
    Q_PROPERTY(QColor dangerSoft READ dangerSoft NOTIFY changed)
    Q_PROPERTY(QColor shadow READ shadow NOTIFY changed)
    Q_PROPERTY(QColor scrim READ scrim NOTIFY changed)

    // The tile a card's artwork is painted on, the same in both shades: a
    // picture does not turn white because the desktop did.
    Q_PROPERTY(QColor artTop READ artTop CONSTANT)
    Q_PROPERTY(QColor artMiddle READ artMiddle CONSTANT)
    Q_PROPERTY(QColor artBottom READ artBottom CONSTANT)
    Q_PROPERTY(QColor artEdge READ artEdge CONSTANT)

    // The ramp the wordmark is set in.
    Q_PROPERTY(QColor ember READ ember CONSTANT)
    Q_PROPERTY(QColor emberDeep READ emberDeep CONSTANT)
    Q_PROPERTY(QColor emberHigh READ emberHigh CONSTANT)

    // The steel the numeral is drawn in, for anything laid over the tile.
    Q_PROPERTY(QColor steel READ steel CONSTANT)

    // Good and bad news for the tile. The pair the rest of the interface uses
    // turns dark in the light shade, which the card art never does.
    Q_PROPERTY(QColor artSuccess READ artSuccess CONSTANT)
    Q_PROPERTY(QColor artDanger READ artDanger CONSTANT)

    /*
    Eight steps, and nothing writes a size of its own. The smallest is what
    a label or a path is set in, so it is where the ladder starts rather
    than where it trails off.
    */
    Q_PROPERTY(int fontTiny READ fontTiny CONSTANT)
    Q_PROPERTY(int fontSmall READ fontSmall CONSTANT)
    Q_PROPERTY(int fontBody READ fontBody CONSTANT)
    Q_PROPERTY(int fontMedium READ fontMedium CONSTANT)
    Q_PROPERTY(int fontLarge READ fontLarge CONSTANT)
    Q_PROPERTY(int fontTitle READ fontTitle CONSTANT)
    Q_PROPERTY(int fontDisplay READ fontDisplay CONSTANT)
    Q_PROPERTY(int fontHero READ fontHero CONSTANT)

    /*
    How heavy a heading is set. Dark ink on a light ground already looks
    heavier than light ink on a dark one at the same weight, so the light
    shade takes a heading one step down rather than matching the number.
    */
    Q_PROPERTY(int headingWeight READ headingWeight NOTIFY changed)

    // The height of anything a line of text sits in on its own.
    Q_PROPERTY(int control READ control CONSTANT)
    Q_PROPERTY(int controlSmall READ controlSmall CONSTANT)

    /*
    The narrowest a full sized button is drawn. Left to their labels, two
    buttons beside each other come out two different widths for no reason
    other than the words in them, so anything short is padded to this.
    */
    Q_PROPERTY(int buttonWidth READ buttonWidth CONSTANT)

    /*
    How far a page is allowed to run. A window can be pulled out as wide as
    the screen goes, and a page that follows it that far puts a foot of
    nothing between a thing and the control for it, so a page stops here and
    sits in the middle of whatever is left over.
    */
    Q_PROPERTY(int pageWidth READ pageWidth CONSTANT)

    // The shelf is laid out from these rather than from a column count, so a
    // wider window fits another card in instead of stretching the ones it has.
    Q_PROPERTY(int cardWidth READ cardWidth CONSTANT)
    Q_PROPERTY(int cardArt READ cardArt CONSTANT)

    Q_PROPERTY(int radius READ radius CONSTANT)
    Q_PROPERTY(int radiusSmall READ radiusSmall CONSTANT)
    Q_PROPERTY(int radiusLarge READ radiusLarge CONSTANT)
    Q_PROPERTY(int gap READ gap CONSTANT)
    Q_PROPERTY(int pad READ pad CONSTANT)
    Q_PROPERTY(QString mono READ mono CONSTANT)

public:
    /*
    The saved mode is read here rather than in the body: members go up in
    declaration order, so resolving the shade after the body had set the mode
    would have resolved it against "system" and left the colours following the
    desktop however the mode was saved.
    */
    explicit Theme(QObject *parent = nullptr)
        : QObject(parent), _mode(saved()), _dark(resolve()) {
        // Only a theme that is following the desktop has anything to follow it to.
        connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
            if (_mode == QLatin1String("system")) {
                apply();
            }
        });
    }

    [[nodiscard]] bool dark() const { return _dark; }
    [[nodiscard]] QString mode() const { return _mode; }

    void setMode(const QString &mode) {
        if (mode == _mode
            || (mode != QLatin1String("system")
                && mode != QLatin1String("light")
                && mode != QLatin1String("dark"))) {
            return;
        }

        _mode = mode;
        apply();
        persist();
    }

    // The one control for it is a single button, so the three modes are a ring.
    Q_INVOKABLE void cycle() {
        QString mode;

        if (_mode == QLatin1String("system")) {
            mode = QStringLiteral("light");
        } else if (_mode == QLatin1String("light")) {
            mode = QStringLiteral("dark");
        } else {
            mode = QStringLiteral("system");
        }

        setMode(mode);
    }

    // The mark's three tile stops in the dark shade; in the light one, the cool
    // grey the steel is drawn out of rather than white.
    [[nodiscard]] QColor background() const   { return QColor(_dark ? 0x0a0d13 : 0xe8ecf4); }
    [[nodiscard]] QColor surface() const      { return QColor(_dark ? 0x151b26 : 0xffffff); }
    [[nodiscard]] QColor raised() const       { return QColor(_dark ? 0x1d2431 : 0xffffff); }
    [[nodiscard]] QColor sunken() const       { return QColor(_dark ? 0x0e131c : 0xdee4ef); }

    /*
    The ground of anything that can be typed into. It is a step past the
    inset panel rather than the same colour as it, because a field is as
    often inside such a panel as on a card, and there it would otherwise be
    the panel's own shade and read as nothing at all.
    */
    [[nodiscard]] QColor field() const        { return QColor(_dark ? 0x07090e : 0xdae1ed); }

    /*
    What hovering lays over whatever is underneath. In the light shade a
    raised surface is already white, so there is nothing lighter to go to and
    a wash of its own is the only thing that shows; being a wash rather than
    a colour, it reads the same over a card, an inset panel or nothing.
    */
    [[nodiscard]] QColor hover() const        { return QColor::fromRgba(_dark ? 0x16ffffff : 0x140d1628); }
    [[nodiscard]] QColor border() const       { return QColor(_dark ? 0x232c3a : 0xccd5e4); }
    [[nodiscard]] QColor borderStrong() const { return QColor(_dark ? 0x3b4759 : 0xa9b6c9); }

    // Steel, in the weights the numeral is shaded with.
    [[nodiscard]] QColor text() const         { return QColor(_dark ? 0xe4eaf5 : 0x101620); }
    [[nodiscard]] QColor muted() const        { return QColor(_dark ? 0xa6b4cd : 0x46536a); }
    [[nodiscard]] QColor faint() const        { return QColor(_dark ? 0x7a8cac : 0x64728a); }

    // The ramp's top end on the dark ground; further down it on a white one,
    // where that end is too light to set a word in.
    [[nodiscard]] QColor accent() const       { return QColor(_dark ? 0xff7a1a : 0xc9450a); }
    [[nodiscard]] QColor accentHover() const  { return QColor(_dark ? 0xff9422 : 0xa83606); }

    // The bright end of the ramp takes dark ink; the deep end takes white.
    [[nodiscard]] QColor accentText() const   { return QColor(_dark ? 0x1a0a02 : 0xffffff); }
    [[nodiscard]] QColor accentSoft() const   { return QColor(_dark ? 0x2b1607 : 0xffe9dc); }

    /*
    What a badge that carries no colour is washed in. The inset panel would
    be the obvious shade for it, but such a badge is nearly always sitting
    on one, and then the two are the same and only the outline is left to
    say where the badge is.
    */
    [[nodiscard]] QColor mutedSoft() const    { return QColor(_dark ? 0x212a37 : 0xdde4ef); }
    [[nodiscard]] QColor success() const      { return QColor(_dark ? 0x52d18b : 0x0a7d4e); }
    [[nodiscard]] QColor successSoft() const  { return QColor(_dark ? 0x0f2b1e : 0xe0f6ec); }

    // Both pulled clear of the ember, so that neither is mistaken for the
    // colour that means "this is the thing to press".
    [[nodiscard]] QColor warning() const      { return QColor(_dark ? 0xffc44d : 0x8a5a08); }
    [[nodiscard]] QColor warningSoft() const  { return QColor(_dark ? 0x332609 : 0xfcf0d8); }
    [[nodiscard]] QColor danger() const       { return QColor(_dark ? 0xff5470 : 0xc22a45); }
    [[nodiscard]] QColor dangerSoft() const   { return QColor(_dark ? 0x35121c : 0xfde7ec); }

    // Nearly opaque in the dark shade, where there is nothing lighter than the
    // card to cast onto.
    [[nodiscard]] QColor shadow() const       { return QColor::fromRgba(_dark ? 0xa8000000 : 0x2e12203a); }

    // What a sheet lays over the window it covers.
    [[nodiscard]] QColor scrim() const        { return QColor::fromRgba(_dark ? 0xbe030509 : 0x780c1420); }

    [[nodiscard]] static QColor artTop()      { return QColor(0x1d2431); }
    [[nodiscard]] static QColor artMiddle()   { return QColor(0x121722); }
    [[nodiscard]] static QColor artBottom()   { return QColor(0x0a0d13); }
    [[nodiscard]] static QColor artEdge()     { return QColor(0x0b0e14); }

    [[nodiscard]] static QColor ember()       { return QColor(0xff5a00); }
    [[nodiscard]] static QColor emberDeep()   { return QColor(0x8f1b06); }
    [[nodiscard]] static QColor emberHigh()   { return QColor(0xff9422); }
    [[nodiscard]] static QColor steel()       { return QColor(0xa6b4cd); }
    [[nodiscard]] static QColor artSuccess()  { return QColor(0x52d18b); }
    [[nodiscard]] static QColor artDanger()   { return QColor(0xff6b80); }

    [[nodiscard]] int headingWeight() const { return _dark ? QFont::Bold : QFont::DemiBold; }

    [[nodiscard]] static int fontTiny() { return 12; }
    [[nodiscard]] static int fontSmall() { return 13; }
    [[nodiscard]] static int fontBody() { return 14; }
    [[nodiscard]] static int fontMedium() { return 15; }
    [[nodiscard]] static int fontLarge() { return 17; }
    [[nodiscard]] static int fontTitle() { return 19; }
    [[nodiscard]] static int fontDisplay() { return 23; }
    [[nodiscard]] static int fontHero() { return 44; }

    [[nodiscard]] static int control() { return 42; }
    [[nodiscard]] static int controlSmall() { return 32; }
    [[nodiscard]] static int buttonWidth() { return 140; }
    [[nodiscard]] static int pageWidth() { return 1180; }

    [[nodiscard]] static int cardWidth() { return 244; }
    [[nodiscard]] static int cardArt() { return 138; }

    [[nodiscard]] static int radius() { return 12; }
    [[nodiscard]] static int radiusSmall() { return 8; }
    [[nodiscard]] static int radiusLarge() { return 18; }
    [[nodiscard]] static int gap() { return 12; }
    [[nodiscard]] static int pad() { return 20; }

    [[nodiscard]] static QString mono() {
        for (const auto *family : {"JetBrains Mono", "Fira Code", "DejaVu Sans Mono", "Noto Sans Mono"}) {
            if (QFontDatabase::families().contains(QString::fromLatin1(family))) {
                return QString::fromLatin1(family);
            }
        }

        return QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
    }

signals:
    void changed();

private:
    // The mode the config was left on, or the desktop's when it names none.
    [[nodiscard]] static QString saved() {
        const QString mode = QString::fromStdString(Session::get().config().general.theme);

        return mode == QLatin1String("light") || mode == QLatin1String("dark")
            ? mode
            : QStringLiteral("system");
    }

    [[nodiscard]] bool resolve() const {
        if (_mode == QLatin1String("light")) {
            return false;
        }

        if (_mode == QLatin1String("dark")) {
            return true;
        }

        return QGuiApplication::styleHints()->colorScheme() != Qt::ColorScheme::Light;
    }

    void apply() {
        // The mode can change without the shade doing so, and the button shows the mode.
        _dark = resolve();
        emit changed();
    }

    /*
    Held in the config the rest of the settings live in, and written straight
    out: a shade is chosen by clicking one button, and there is no Save beside
    it to press afterwards.
    */
    void persist() const {
        Session &session = Session::get();

        session.config().general.theme = _mode.toStdString();
        session.save();
    }

    QString _mode = QStringLiteral("system");
    bool _dark = true;
};
