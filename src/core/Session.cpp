/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
 * Copyright (C) 2018-2019  Lcferrum
 * Copyright (C) 2023-2026  spacebub
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

#include <array>
#include <chrono>
#include <ctime>
#include <utility>

#include "core/Import.h"
#include "core/Paths.h"
#include "core/Session.h"
#include "core/Text.h"

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

    return sibling.replace_extension(".json");
}

// The time, written the one way anything here writes it. By hand rather than
// through the formatting library, which is a good deal of the standard library
// pulled in for one line of one field.
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
        /*
        Written out at once, so the .json is what is read from here on. A read
        that is only asking a question of the file leaves it as it found it: a
        config that says to stay out of the way is not one to write a new
        format of.
        */
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

    // The open config is the user config, so the flag goes with everything else
    // it is holding and is written whenever that is.
    if (_path == json) {
        _config.general.noUserConf = value;

        return true;
    }

    Config user;

    // Nothing there to read: only turning the flag on is worth a file of its own.
    if (!read(json, ini, user, false) && !value) {
        return true;
    }

    user.general.noUserConf = value;

    return user.save(json, error);
}

std::vector<std::string> Session::start(const std::vector<std::string> &arguments) {
    std::vector<std::string> rest;

    // A config named on the command line wins over everything else.
    for (const std::string &argument : arguments) {
        if (Text::iendsWith(argument, ".json")) {
            _path = argument;
            _source = Source::UserSpecified;
        } else if (Text::iendsWith(argument, ".ini")) {
            // Legacy configs still work; they migrate to a .json beside them.
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

    // What the probe below read, kept rather than read again: where it is the
    // config being opened, the second read is the same file for the same
    // answer. Only where a legacy config still has to be migrated is it not.
    Config probed;
    bool reuseProbe = false;

    if (_path.empty()) {
        const std::filesystem::path userJson = paths.configPath(Paths::USER);
        const std::filesystem::path userIni = firstExisting(paths.legacyConfigPath(Paths::USER));
        const bool json = hasContent(userJson);

        if (json || !userIni.empty()) {
            /*
            A legacy config is read without being migrated, so the check below
            sees the setting whichever format it came from and a config that is
            being passed over is left exactly as it was found.
            */
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

    /*
    Portable mode: a config sitting next to the executable. On platforms where
    that is already the per user location, this step has nothing to add, and
    skipping it keeps the noUserConf check above authoritative.
    */
    if (_path.empty() && paths.configPath(Paths::USER).parent_path() != Paths::executableDirectory()) {
        const std::filesystem::path directory = Paths::executableDirectory();
        std::error_code code;

        if (std::filesystem::exists(directory / "zdl.json", code)) {
            _path = directory / "zdl.json";
            _source = Source::Portable;
        } else if (std::filesystem::exists(directory / "zdl.ini", code)) {
            _legacy = directory / "zdl.ini";
            _path = directory / "zdl.json";
            _source = Source::Portable;
        }
    }

    /*
    Nothing else was found. The user config is opened even when it asked to be
    passed over, since a config that stands aside for one that is not there
    leaves nothing to run on at all.
    */
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

    /*
    A .zdl on the command line becomes a profile of its own, and what it brings
    with it replaces the remembered file list rather than adding to it.
    */
    bool replaceFiles = true;

    for (auto it = rest.begin(); it != rest.end();) {
        if (!Text::iendsWith(*it, ".zdl")) {
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

    // Files to load and no profile to put them in: they are what the new one is.
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

    if (Text::iendsWith(path.string(), ".ini")) {
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
