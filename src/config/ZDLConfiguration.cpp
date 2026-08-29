/*
 * This file is part of qZDL
 * Copyright (C) 2007-2012  Cody Harris
 * Copyright (C) 2019  Lcferrum
 * Copyright (C) 2023-2026  spacebub
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

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

#include "config/ZDLConfiguration.h"

namespace {

const char *CONFIG_FILE_NAME = "zdl.json";
const char *LEGACY_FILE_NAME = "zdl.ini";

#ifdef _WIN32

/** Directory holding the running executable. */
QString execDir() {
    QString dir = QCoreApplication::applicationDirPath();
    return dir.isEmpty() ? QDir::currentPath() : dir;
}

#else

const char *CONFIG_DIR_NAME = "qzdl";

/**
 * Per user config directory: $XDG_CONFIG_HOME/qzdl, falling back to
 * ~/.config/qzdl.  GenericConfigLocation is exactly $XDG_CONFIG_HOME on Unix.
 */
QString xdgConfigDir() {
    QString base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + "/.config";
    }
    return base + "/" + CONFIG_DIR_NAME;
}

/** Machine wide config directory, /etc/xdg/qzdl on Unix. */
QString xdgSystemConfigDir() {
    QStringList bases = QStandardPaths::standardLocations(QStandardPaths::GenericConfigLocation);
    // The writable per user location comes first; the system wide ones follow.
    if (bases.size() > 1) {
        return bases.last() + "/" + CONFIG_DIR_NAME;
    }
    return {};
}

#endif

}

ZDLConfiguration::ZDLConfiguration() {
    // QSettings is used purely to reproduce the paths pre-JSON versions of ZDL
    // used to pick.  Nothing is read or written through it.
    QSettings const system(QSettings::IniFormat, QSettings::SystemScope, "Vectec Software", "qZDL", nullptr);
    QSettings const user(QSettings::IniFormat, QSettings::UserScope, "Vectec Software", "qZDL", nullptr);

    QString userDir;
    QString systemDir;

#ifdef _WIN32
    // ZDL ships on Windows as a single portable exe, so a new install keeps its
    // config right beside the exe: AppData and Documents are not places anyone
    // thinks to look.  But if an older ZDL already put a config under AppData,
    // that folder stays in use rather than the config being relocated.
    QString appDataDir = QFileInfo(user.fileName()).absolutePath();
    bool appDataInUse = QFileInfo::exists(user.fileName())
                        || QFileInfo::exists(appDataDir + "/" + CONFIG_FILE_NAME);

    if (appDataInUse) {
        userDir = appDataDir;
        legacyPaths[CONF_USER] << user.fileName();
    } else {
        userDir = execDir();
        legacyPaths[CONF_USER] << execDir() + "/" + LEGACY_FILE_NAME;
    }
#else
    userDir = xdgConfigDir();
    systemDir = xdgSystemConfigDir();

    // A zdl.ini beside the exe stays a portable config and is picked up as one;
    // migrating it into the XDG directory here would break that.
    legacyPaths[CONF_USER] << user.fileName();
#endif

    paths[CONF_USER] = userDir + "/" + CONFIG_FILE_NAME;
    paths[CONF_SYSTEM] = systemDir.isEmpty() ? QString() : systemDir + "/" + CONFIG_FILE_NAME;
    paths[CONF_FILE] = CONFIG_FILE_NAME;

    legacyPaths[CONF_SYSTEM] << system.fileName();
    legacyPaths[CONF_FILE] << LEGACY_FILE_NAME;
}

QString ZDLConfiguration::getPath(const ConfScope scope) const {
    if (scope >= NUM_CONFS) {
        return {};
    }
    return paths[scope];
}

QStringList ZDLConfiguration::getLegacyPaths(const ConfScope scope) const {
    if (scope >= NUM_CONFS) {
        return {};
    }
    return legacyPaths[scope];
}
