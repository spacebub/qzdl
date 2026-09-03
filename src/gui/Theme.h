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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <QColor>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QObject>
#include <QStyleHints>
#include <QtQml/qqmlregistration.h>

#include "core/Session.h"

/*
One palette, in two shades, and one ladder of sizes. Everything the interface
paints and every size it sets comes from here, so the whole application
changes with a single value.
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

    Q_PROPERTY(int radius READ radius CONSTANT)
    Q_PROPERTY(int radiusSmall READ radiusSmall CONSTANT)
    Q_PROPERTY(int radiusLarge READ radiusLarge CONSTANT)
    Q_PROPERTY(int gap READ gap CONSTANT)
    Q_PROPERTY(int pad READ pad CONSTANT)
    Q_PROPERTY(QString mono READ mono CONSTANT)

public:
    explicit Theme(QObject *parent = nullptr) : QObject(parent) {
        _mode = QString::fromStdString(Session::get().config().general.theme);
        _dark = resolve();

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
        setMode(_mode == QLatin1String("system") ? QStringLiteral("light")
              : _mode == QLatin1String("light")  ? QStringLiteral("dark")
                                                 : QStringLiteral("system"));
    }

    [[nodiscard]] QColor background() const   { return _dark ? "#0e1116" : "#f4f6fa"; }
    [[nodiscard]] QColor surface() const      { return _dark ? "#151a22" : "#ffffff"; }
    [[nodiscard]] QColor raised() const       { return _dark ? "#1b2029" : "#ffffff"; }
    [[nodiscard]] QColor sunken() const       { return _dark ? "#0b0e13" : "#eef1f7"; }

    /*
    The ground of anything that can be typed into. It is a step past the
    inset panel rather than the same colour as it, because a field is as
    often inside such a panel as on a card, and there it would otherwise be
    the panel's own shade and read as nothing at all.
    */
    [[nodiscard]] QColor field() const        { return _dark ? "#04060a" : "#e2e7f0"; }

    /*
    What hovering lays over whatever is underneath. In the light shade a
    raised surface is already white, so there is nothing lighter to go to and
    a wash of its own is the only thing that shows; being a wash rather than
    a colour, it reads the same over a card, an inset panel or nothing.
    */
    [[nodiscard]] QColor hover() const        { return _dark ? QColor(255, 255, 255, 20) : QColor(10, 20, 40, 20); }
    [[nodiscard]] QColor border() const       { return _dark ? "#262f3c" : "#dde3ee"; }
    [[nodiscard]] QColor borderStrong() const { return _dark ? "#3d4b5d" : "#c3ccdd"; }
    [[nodiscard]] QColor text() const         { return _dark ? "#e8ecf4" : "#141922"; }
    [[nodiscard]] QColor muted() const        { return _dark ? "#b3bdcd" : "#4d5769"; }
    [[nodiscard]] QColor faint() const        { return _dark ? "#8d99ad" : "#67738a"; }
    [[nodiscard]] QColor accent() const       { return _dark ? "#5b8cff" : "#3568f0"; }
    [[nodiscard]] QColor accentHover() const  { return _dark ? "#7ba2ff" : "#2a58d8"; }
    static        QColor accentText()         { return 0xffffff; }
    [[nodiscard]] QColor accentSoft() const   { return _dark ? "#1a2440" : "#e6edff"; }

    /*
    What a badge that carries no colour is washed in. The inset panel would
    be the obvious shade for it, but such a badge is nearly always sitting
    on one, and then the two are the same and only the outline is left to
    say where the badge is.
    */
    [[nodiscard]] QColor mutedSoft() const    { return _dark ? "#232b37" : "#e3e8f1"; }
    [[nodiscard]] QColor success() const      { return _dark ? "#43d391" : "#0a7d4e"; }
    [[nodiscard]] QColor successSoft() const  { return _dark ? "#12301f" : "#e2f7ee"; }
    [[nodiscard]] QColor warning() const      { return _dark ? "#f0b429" : "#96620f"; }
    [[nodiscard]] QColor warningSoft() const  { return _dark ? "#33270c" : "#fdf2dc"; }
    [[nodiscard]] QColor danger() const       { return _dark ? "#f4657a" : "#c62d46"; }
    [[nodiscard]] QColor dangerSoft() const   { return _dark ? "#341219" : "#fde8eb"; }
    [[nodiscard]] QColor shadow() const       { return _dark ? "#00000066" : "#12203a1f"; }

    // What a sheet lays over the window it covers.
    [[nodiscard]] QColor scrim() const        { return _dark ? QColor(3, 5, 9, 168) : QColor(12, 20, 32, 110); }

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
