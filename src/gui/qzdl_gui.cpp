/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
 * Copyright (C) 2018-2019  Lcferrum
 * Copyright (C) 2023-2026  spacebub
 *
 * Entry point for the graphical interface
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

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

#include "core/Launcher.h"
#include "core/Paths.h"
#include "core/Session.h"

int main(int argc, char *argv[]) {
    QGuiApplication::setApplicationName("ZDL");
    QGuiApplication::setApplicationDisplayName("ZDL");
    QGuiApplication::setApplicationVersion(QZDL_VERSION);
    QGuiApplication::setOrganizationName("qzdl");

    QGuiApplication::setDesktopFileName("qzdl");

    const QGuiApplication application(argc, argv);

    QGuiApplication::setWindowIcon(QIcon::fromTheme("qzdl", QIcon(":/qzdl-256.png")));

    // The interface brings its own look, the platform style stays out of it.
    QQuickStyle::setStyle("Basic");

    Paths::setExecutable(QCoreApplication::applicationFilePath().toStdString());

    std::vector<std::string> arguments;

    for (int index = 1; index < argc; index++) {
        arguments.emplace_back(argv[index]);
    }

    Session &session = Session::get();

    session.start(arguments);

    /*
    A .zdl handed over on the command line, with the setting for it turned on,
    is the one path that never shows a window: the file says what to launch, so
    it is launched and that is the whole run.
    */
    if (session.openedZdlFile() && session.config().general.launchZdlImmediately) {
        std::string error;

        if (Launcher::launch(session.config(), nullptr, nullptr, &error)) {
            return 0;
        }

        qWarning("Nothing was launched: %s", error.c_str());

        return 1;
    }

    QQmlApplicationEngine engine;

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &application,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);

    engine.loadFromModule("Zdl.App", "Main");

    return QGuiApplication::exec();
}
