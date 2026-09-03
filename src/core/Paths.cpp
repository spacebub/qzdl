/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
 * Copyright (C) 2019  Lcferrum
 * Copyright (C) 2023-2026  spacebub
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

#include <cstdlib>

#include "core/Paths.h"

namespace {

const char *CONFIG_FILE_NAME = "zdl.json";
const char *LEGACY_FILE_NAME = "zdl.ini";

std::filesystem::path executablePath;

/** An environment variable as a path, empty when it is unset or blank. */
std::filesystem::path fromEnvironment(const char *name) {
    // NOLINTNEXTLINE(concurrency-mt-unsafe) -- read once, before any threads.
    const char *value = std::getenv(name);

    return value != nullptr && *value != '\0' ? std::filesystem::path(value) : std::filesystem::path();
}

#ifndef _WIN32

const char *CONFIG_DIR_NAME = "qzdl";

/**
 * Per user config directory: $XDG_CONFIG_HOME/qzdl, falling back to
 * ~/.config/qzdl when the variable is unset, which is what the spec says it
 * defaults to.
 */
std::filesystem::path xdgConfigDir() {
    std::filesystem::path base = fromEnvironment("XDG_CONFIG_HOME");

    if (base.empty()) {
        const std::filesystem::path home = Paths::home();

        if (home.empty()) {
            return {};
        }

        base = home / ".config";
    }

    return base / CONFIG_DIR_NAME;
}

/** Machine wide config directory: the last of $XDG_CONFIG_DIRS, /etc/xdg by default. */
std::filesystem::path xdgSystemConfigDir() {
    // NOLINTNEXTLINE(concurrency-mt-unsafe) -- read once, before any threads.
    const char *dirs = std::getenv("XDG_CONFIG_DIRS");
    std::string base = dirs != nullptr && *dirs != '\0' ? dirs : "/etc/xdg";

    // Only the last one is used, matching where earlier versions of ZDL looked.
    if (const size_t last = base.find_last_of(':'); last != std::string::npos) {
        base = base.substr(last + 1);
    }

    return base.empty() ? std::filesystem::path() : std::filesystem::path(base) / CONFIG_DIR_NAME;
}

/*
Where pre-JSON versions of ZDL put their INI, which was wherever Qt's own
QSettings put a file for "Vectec Software"/"qZDL". Nothing is written there
any more; it is read once so an existing config can be carried over.
*/
std::filesystem::path legacyUserIni() {
    const std::filesystem::path base = xdgConfigDir().parent_path();

    return base.empty() ? std::filesystem::path() : base / "Vectec Software" / "qZDL.conf";
}

std::filesystem::path legacySystemIni() {
    const std::filesystem::path base = xdgSystemConfigDir().parent_path();

    return base.empty() ? std::filesystem::path() : base / "Vectec Software" / "qZDL.conf";
}

#endif

}

void Paths::setExecutable(const std::filesystem::path &path) {
    std::error_code code;
    std::filesystem::path resolved = std::filesystem::weakly_canonical(path, code);

    executablePath = code ? path : resolved;
}

const std::filesystem::path &Paths::executable() {
    return executablePath;
}

std::filesystem::path Paths::executableDirectory() {
    if (executablePath.empty()) {
        std::error_code code;
        std::filesystem::path here = std::filesystem::current_path(code);

        return code ? std::filesystem::path(".") : here;
    }

    return executablePath.parent_path();
}

std::filesystem::path Paths::home() {
#ifdef _WIN32
    if (std::filesystem::path profile = fromEnvironment("USERPROFILE"); !profile.empty()) {
        return profile;
    }

    const std::filesystem::path drive = fromEnvironment("HOMEDRIVE");
    const std::filesystem::path rest = fromEnvironment("HOMEPATH");

    return drive.empty() || rest.empty() ? std::filesystem::path() : drive / rest;
#else
    return fromEnvironment("HOME");
#endif
}

const Paths &Paths::get() {
    static const Paths instance;

    return instance;
}

Paths::Paths() {
    std::filesystem::path userDir;
    std::filesystem::path systemDir;

#ifdef _WIN32
    /*
    ZDL ships on Windows as a single portable exe, so a new install keeps its
    config right beside the exe: AppData and Documents are not places anyone
    thinks to look. But if an older ZDL already put a config under AppData,
    that folder stays in use rather than the config being relocated.
    */
    const std::filesystem::path appData = fromEnvironment("APPDATA");
    const std::filesystem::path appDataDir = appData.empty()
        ? std::filesystem::path()
        : appData / "Vectec Software" / "qZDL";
    const std::filesystem::path appDataIni = appDataDir.empty()
        ? std::filesystem::path()
        : appDataDir / "qZDL.ini";

    std::error_code code;
    const bool appDataInUse = !appDataDir.empty()
        && (std::filesystem::exists(appDataIni, code)
            || std::filesystem::exists(appDataDir / CONFIG_FILE_NAME, code));

    if (appDataInUse) {
        userDir = appDataDir;
        _legacy[USER].push_back(appDataIni);
    } else {
        userDir = executableDirectory();
        _legacy[USER].push_back(userDir / LEGACY_FILE_NAME);
    }

    if (const std::filesystem::path programData = fromEnvironment("PROGRAMDATA"); !programData.empty()) {
        _legacy[SYSTEM].push_back(programData / "Vectec Software" / "qZDL" / "qZDL.ini");
    }
#else
    userDir = xdgConfigDir();
    systemDir = xdgSystemConfigDir();

    /*
    A zdl.ini beside the exe stays a portable config and is picked up as one;
    migrating it into the XDG directory here would break that.
    */
    if (std::filesystem::path ini = legacyUserIni(); !ini.empty()) {
        _legacy[USER].push_back(std::move(ini));
    }

    if (std::filesystem::path ini = legacySystemIni(); !ini.empty()) {
        _legacy[SYSTEM].push_back(std::move(ini));
    }
#endif

    _paths[USER] = userDir.empty() ? std::filesystem::path(CONFIG_FILE_NAME) : userDir / CONFIG_FILE_NAME;
    _paths[SYSTEM] = systemDir.empty() ? std::filesystem::path() : systemDir / CONFIG_FILE_NAME;
    _paths[FILE] = CONFIG_FILE_NAME;

    _legacy[FILE].emplace_back(LEGACY_FILE_NAME);
}

std::filesystem::path Paths::path(const Scope scope) const {
    return scope < NUM_CONFS ? _paths[scope] : std::filesystem::path();
}

std::vector<std::filesystem::path> Paths::legacy(const Scope scope) const {
    return scope < NUM_CONFS ? _legacy[scope] : std::vector<std::filesystem::path>();
}
