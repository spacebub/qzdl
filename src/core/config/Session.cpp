/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
 * Copyright (C) 2018-2019  Lcferrum
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

#include <array>
#include <chrono>
#include <ctime>
#include <utility>

#include "core/config/Import.h"
#include "core/config/Schema.h"
#include "core/config/Session.h"
#include "core/system/Paths.h"
#include "core/util/Text.h"

namespace {

bool hasContent(const std::filesystem::path &path) {
    std::error_code code;

    return std::filesystem::is_regular_file(path, code) && std::filesystem::file_size(path, code) > 20;
}

std::filesystem::path firstExisting(const std::vector<std::filesystem::path> &paths) {
    for (const std::filesystem::path &path : paths) {
        if (hasContent(path)) {
            return path;
        }
    }

    return {};
}

std::filesystem::path jsonSiblingOf(const std::filesystem::path &ini) {
    std::filesystem::path sibling = ini;

    return sibling.replace_extension(ConfigFile::JSON_EXT);
}

std::string nowInUtc() {
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm broken{};

#ifdef _WIN32
    gmtime_s(&broken, &now);
#else
    gmtime_r(&now, &broken);
#endif

    std::array<char, 32> written{};
    const size_t length = std::strftime(written.data(), written.size(), "%Y-%m-%dT%H:%M:%SZ",
                                        &broken);

    return {written.data(), length};
}

}

Session &Session::get() {
    static Session instance;

    return instance;
}

Config &Session::config() {
    return _config;
}

const Config &Session::config() const {
    return _config;
}

const std::filesystem::path &Session::path() const {
    return _path;
}

Session::Source Session::source() const {
    return _source;
}

bool Session::openedZdlFile() const {
    return _openedZdl;
}

std::pair<std::filesystem::path, std::filesystem::path> Session::userPaths() {
    const Paths &paths = Paths::get();

    return {paths.configPath(Paths::USER), firstExisting(paths.legacyConfigPath(Paths::USER))};
}

bool Session::read(const std::filesystem::path &jsonPath,
                   const std::filesystem::path &iniPath,
                   Config &into,
                   const bool migrate) {
    std::error_code code;

    if (std::filesystem::exists(jsonPath, code)) {
        return into.load(jsonPath);
    }

    if (!iniPath.empty() && Import::loadLegacyFile(iniPath, into)) {
        if (migrate) {
            into.save(jsonPath);
        }

        return true;
    }

    return false;
}

bool Session::userConfigIgnored() const {
    const auto [json, ini] = userPaths();

    if (_path == json) {
        return _config.general.noUserConf;
    }

    Config probe;

    return read(json, ini, probe, false) && probe.general.noUserConf;
}

bool Session::setUserConfigIgnored(const bool value, std::string *error) {
    const auto [json, ini] = userPaths();

    if (_path == json) {
        _config.general.noUserConf = value;

        return true;
    }

    Config user;

    // No user config yet, and off needs no file.
    if (!read(json, ini, user, false) && !value) {
        return true;
    }

    user.general.noUserConf = value;

    return user.save(json, error);
}

std::vector<std::string> Session::start(const std::vector<std::string> &arguments) {
    std::vector<std::string> rest;

    for (const std::string &argument : arguments) {
        if (Text::iendsWith(argument, ConfigFile::JSON_EXT)) {
            _path = argument;
            _source = Source::UserSpecified;
        } else if (Text::iendsWith(argument, ConfigFile::INI_EXT)) {
            _legacy = argument;
            _path = jsonSiblingOf(argument);
            _source = Source::UserSpecified;
        } else if (!argument.starts_with("-")) {
            rest.push_back(argument);

            continue;
        } else {
            continue;
        }

        break;
    }

    const Paths &paths = Paths::get();

    // The probe's read is reused when it turns out to be the config being opened.
    Config probed;
    bool reuseProbe = false;

    if (_path.empty()) {
        const std::filesystem::path userJson = paths.configPath(Paths::USER);
        const std::filesystem::path userIni = firstExisting(paths.legacyConfigPath(Paths::USER));
        const bool json = hasContent(userJson);

        if (json || !userIni.empty()) {
            Config probe;

            if (read(userJson, userIni, probe, false) && !probe.general.noUserConf) {
                _path = userJson;
                _legacy = userIni;
                _source = Source::User;
                reuseProbe = json;
                probed = std::move(probe);
            }
        }
    }

    // Portable mode: a config beside the executable, unless that is already the user location.
    if (_path.empty() && paths.configPath(Paths::USER).parent_path() != Paths::executableDirectory()) {
        const std::filesystem::path directory = Paths::executableDirectory();
        std::error_code code;

        if (std::filesystem::exists(directory / ConfigFile::JSON, code)) {
            _path = directory / ConfigFile::JSON;
            _source = Source::Portable;
        } else if (std::filesystem::exists(directory / ConfigFile::INI, code)) {
            _legacy = directory / ConfigFile::INI;
            _path = directory / ConfigFile::JSON;
            _source = Source::Portable;
        }
    }

    // Fallback: the user config, even one flagged noUserConf.
    if (_path.empty()) {
        _path = paths.configPath(Paths::USER);
        _legacy = firstExisting(paths.legacyConfigPath(Paths::USER));
        _source = Source::Fallback;
    }

    if (reuseProbe) {
        _config = std::move(probed);
    } else {
        read(_path, _legacy, _config, true);
    }

    // A .zdl becomes its own profile; loose files replace the active profile's list.
    bool replaceFiles = true;

    for (auto it = rest.begin(); it != rest.end();) {
        if (!Text::iendsWith(*it, ConfigFile::ZDL_EXT)) {
            ++it;

            continue;
        }

        Profile profile;

        if (Import::loadZdlFile(*it, profile)) {
            profile.name = _config.uniqueProfileName(profile.name);
            _config.profiles.push_back(profile);
            _config.ensureConfigFiles();
            _config.setActiveProfile(profile.id);
            _openedZdl = true;
            replaceFiles = false;
        }

        it = rest.erase(it);

        break;
    }

    if (!rest.empty() && _config.profiles.empty()) {
        _config.setActiveProfile(_config.addProfile({}));
    }

    for (const std::string &file : rest) {
        if (replaceFiles) {
            _config.activeProfile().files.clear();
            replaceFiles = false;
        }

        _config.activeProfile().files.push_back(FileEntry{.file = file, .enabled = true});
    }

    return rest;
}

bool Session::load(const std::filesystem::path &path, std::string *error) {
    Config loaded;

    if (Text::iendsWith(path.string(), ConfigFile::INI_EXT)) {
        if (!Import::loadLegacyFile(path, loaded)) {
            if (error != nullptr) {
                *error = "could not read " + path.string();
            }

            return false;
        }

        _legacy = path;
        _path = jsonSiblingOf(path);
    } else {
        if (!loaded.load(path, error)) {
            return false;
        }

        _legacy.clear();
        _path = path;
    }

    _config = std::move(loaded);
    _source = Source::UserSpecified;

    return true;
}

bool Session::save(std::string *error) const {
    return _config.save(_path, error);
}

bool Session::saveAs(const std::filesystem::path &path, std::string *error) {
    if (!_config.save(path, error)) {
        return false;
    }

    _path = path;
    _legacy.clear();
    _source = Source::UserSpecified;

    return true;
}

bool Session::adoptAsUserConfig(std::string *error) {
    const std::filesystem::path target = Paths::get().configPath(Paths::USER);

    if (target != _path) {
        _config.general.isImported = true;
        _config.general.importedFrom = _path.string();
        _config.general.importDate = nowInUtc();
    }

    if (!_config.save(target, error)) {
        return false;
    }

    _path = target;
    _legacy.clear();
    _source = Source::User;

    return true;
}
