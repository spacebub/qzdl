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
#include <utility>

#include "ttk/system/Env.h"
#include "ttk/system/Paths.h"

#include "core/config/ConfigPaths.h"
#include "core/config/Schema.h"

namespace {

#ifdef _WIN32

std::filesystem::path fromEnvironment(const char *name) {
    const std::string value = ttk::Env::get(name);

    return value.empty() ? std::filesystem::path() : std::filesystem::path(value);
}

#else

std::filesystem::path legacyIni(const std::filesystem::path &dir) {
    const std::filesystem::path base = dir.parent_path();

    return base.empty() ? std::filesystem::path() : base / "Vectec Software" / "qZDL.conf";
}

#endif

}

const ConfigPaths &ConfigPaths::get() {
    static const ConfigPaths instance;

    return instance;
}

ConfigPaths::ConfigPaths() {
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
            || std::filesystem::exists(vendorDir / ConfigFile::JSON, code));

    if (vendorInUse) {
        userDir = vendorDir;
        _legacy[USER].push_back(vendorIni);
    } else if (!appData.empty()) {
        userDir = appData / ttk::Paths::application();
        _legacy[USER].push_back(userDir / ConfigFile::INI);
    } else {
        userDir = ttk::Paths::executable_directory();
        _legacy[USER].push_back(userDir / ConfigFile::INI);
    }

    if (const std::filesystem::path programData = fromEnvironment("PROGRAMDATA"); !programData.empty()) {
        _legacy[SYSTEM].push_back(programData / "Vectec Software" / "qZDL" / "qZDL.ini");
    }
#else
    userDir = ttk::Paths::config_directory();
    systemDir = ttk::Paths::system_config_directory();

    if (std::filesystem::path ini = legacyIni(userDir); !ini.empty()) {
        _legacy[USER].push_back(std::move(ini));
    }

    if (std::filesystem::path ini = legacyIni(systemDir); !ini.empty()) {
        _legacy[SYSTEM].push_back(std::move(ini));
    }
#endif

    _paths[USER] = userDir.empty() ? std::filesystem::path(ConfigFile::JSON) : userDir / ConfigFile::JSON;
    _paths[SYSTEM] = systemDir.empty() ? std::filesystem::path() : systemDir / ConfigFile::JSON;
    _paths[FILE] = ConfigFile::JSON;

    _legacy[FILE].emplace_back(ConfigFile::INI);
}

std::filesystem::path ConfigPaths::configPath(const Scope scope) const {
    return scope < NUM_CONFS ? _paths[scope] : std::filesystem::path();
}

std::vector<std::filesystem::path> ConfigPaths::legacyConfigPath(const Scope scope) const {
    return scope < NUM_CONFS ? _legacy[scope] : std::vector<std::filesystem::path>();
}
