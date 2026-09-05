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

#include <QObject>
#include <QRect>
#include <QtQml/qqmlregistration.h>

#include "gui/ConfigBridge.h"
#include "gui/Notifier.h"
#include "gui/Runs.h"

// The one object the interface talks to.
class App : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString qtVersion READ qtVersion CONSTANT)
    Q_PROPERTY(Notifier *notify READ notify CONSTANT)
    Q_PROPERTY(Runs *runs READ runs CONSTANT)
    Q_PROPERTY(ConfigBridge *config READ config CONSTANT)

    // What the file dialogs put on their filter rows, by kind.
    Q_PROPERTY(QStringList wadFilters READ wadFilters CONSTANT)
    Q_PROPERTY(QStringList portFilters READ portFilters CONSTANT)
    Q_PROPERTY(QStringList zdlFilters READ zdlFilters CONSTANT)
    Q_PROPERTY(QStringList configFilters READ configFilters CONSTANT)
    Q_PROPERTY(QStringList saveFilters READ saveFilters CONSTANT)

    Q_PROPERTY(bool windows READ isWindows CONSTANT)

public:
    explicit App(QObject *parent = nullptr);

    [[nodiscard]] static QString version();
    [[nodiscard]] static QString qtVersion();
    [[nodiscard]] Notifier *notify() const;
    [[nodiscard]] Runs *runs() const;
    [[nodiscard]] ConfigBridge *config() const;

    [[nodiscard]] static QStringList wadFilters();
    [[nodiscard]] static QStringList portFilters();
    [[nodiscard]] static QStringList zdlFilters();
    [[nodiscard]] static QStringList configFilters();
    [[nodiscard]] static QStringList saveFilters();

    [[nodiscard]] static bool isWindows();

    Q_INVOKABLE static QStringList drives();

    Q_INVOKABLE static void copyToClipboard(const QString &text);
    Q_INVOKABLE static bool isDirectory(const QString &path);
    Q_INVOKABLE static bool isFile(const QString &path);
    Q_INVOKABLE static bool reveal(const QString &path);

    // A path reads better with the home directory written the way a shell writes it.
    Q_INVOKABLE static QString prettyPath(const QString &path);

    // Where to point an Image at a game's own title screen, if it has one.
    Q_INVOKABLE static QString artFor(const QString &file);

    // The directory a file sits in, for opening the one rather than the other.
    Q_INVOKABLE static QString directoryOf(const QString &path);

    /*
    Where a file dialog should open, by what it is asking for. Each kind
    remembers where it last landed, so asking for a WAD twice starts where the
    last WAD came from rather than in the same place every time.
    */
    Q_INVOKABLE static QString startDirectory(const QString &kind);

    Q_INVOKABLE void rememberDirectory(const QString &kind, const QString &path);

    // Where the window was left, and where to put it back.
    Q_INVOKABLE static QRect rememberedGeometry();

    Q_INVOKABLE static void rememberGeometry(int x, int y, int width, int height);

    // Written on the way out. A file list that is not being remembered is
    // emptied first, which is what that setting means.
    Q_INVOKABLE static void shutdown();

private:
    Notifier *_notifier;
    Runs *_runs;
    ConfigBridge *_config;
};
