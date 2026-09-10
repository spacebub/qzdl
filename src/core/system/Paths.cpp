/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
 * Copyright (C) 2019  Lcferrum
 * Copyright (C) 2023-2026  spacebub
 *
 * qZDL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <string>

#include "core/system/Env.h"
#include "core/system/Paths.h"

namespace {

const char *CONFIG_FILE_NAME = "zdl.json";
const char *LEGACY_FILE_NAME = "zdl.ini";

// Function-local to sidestep static initialization order.
std::filesystem::path &executablePath() {
    static std::filesystem::path path;

    return path;
}

std::filesystem::path fromEnvironment(const char *name) {
    const std::string value = Env::get(name);

    return value.empty() ? std::filesystem::path() : std::filesystem::path(value);
}

#ifdef _WIN32

const char *CONFIG_DIR_NAME = "qZDL";

#else

const char *CONFIG_DIR_NAME = "qzdl";

std::filesystem::path xdgConfigDir() {
    std::filesystem::path base = fromEnvironment("XDG_CONFIG_HOME");

    if (base.empty()) {
        const std::filesystem::path home = Paths::homeDirectory();

        if (home.empty()) {
            return {};
        }

        base = home / ".config";
    }

    return base / CONFIG_DIR_NAME;
}

// The last entry of $XDG_CONFIG_DIRS, /etc/xdg by default.
std::filesystem::path xdgSystemConfigDir() {
    const std::string dirs = Env::get("XDG_CONFIG_DIRS");
    std::string base = dirs.empty() ? "/etc/xdg" : dirs;

    if (const size_t last = base.find_last_of(':'); last != std::string::npos) {
        base = base.substr(last + 1);
    }

    return base.empty() ? std::filesystem::path() : std::filesystem::path(base) / CONFIG_DIR_NAME;
}

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
    std::filesystem::path const resolved = std::filesystem::weakly_canonical(path, code);

    executablePath() = code ? path : resolved;
}

const std::filesystem::path &Paths::executable() {
    return executablePath();
}

std::filesystem::path Paths::executableDirectory() {
    if (executablePath().empty()) {
        std::error_code code;
        std::filesystem::path const here = std::filesystem::current_path(code);

        return code ? std::filesystem::path(".") : here;
    }

    return executablePath().parent_path();
}

std::filesystem::path Paths::homeDirectory() {
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

std::filesystem::path Paths::dataDirectory() {
#ifdef _WIN32
    if (const std::filesystem::path appData = fromEnvironment("APPDATA"); !appData.empty()) {
        return appData / CONFIG_DIR_NAME;
    }

    return executableDirectory() / CONFIG_DIR_NAME;
#else
    std::filesystem::path base = fromEnvironment("XDG_DATA_HOME");

    if (base.empty()) {
        const std::filesystem::path home = Paths::homeDirectory();

        if (home.empty()) {
            return {};
        }

        base = home / ".local" / "share";
    }

    return base / CONFIG_DIR_NAME;
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
    const std::filesystem::path appData = fromEnvironment("APPDATA");
    const std::filesystem::path vendorDir = appData.empty()
        ? std::filesystem::path()
        : appData / "Vectec Software" / "qZDL";
    const std::filesystem::path vendorIni = vendorDir.empty()
        ? std::filesystem::path()
        : vendorDir / "qZDL.ini";

    std::error_code code;
    const bool vendorInUse = !vendorDir.empty()
        && (std::filesystem::exists(vendorIni, code)
            || std::filesystem::exists(vendorDir / CONFIG_FILE_NAME, code));

    if (vendorInUse) {
        userDir = vendorDir;
        _legacy[USER].push_back(vendorIni);
    } else if (!appData.empty()) {
        userDir = appData / CONFIG_DIR_NAME;
        _legacy[USER].push_back(userDir / LEGACY_FILE_NAME);
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

std::filesystem::path Paths::configPath(const Scope scope) const {
    return scope < NUM_CONFS ? _paths[scope] : std::filesystem::path();
}

std::vector<std::filesystem::path> Paths::legacyConfigPath(const Scope scope) const {
    return scope < NUM_CONFS ? _legacy[scope] : std::vector<std::filesystem::path>();
}
