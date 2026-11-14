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

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>

#include "ttk/system/Paths.h"
#include "ttk/system/Text.h"

#include "core/config/ConfigPaths.h"
#include "core/config/Import.h"
#include "core/config/Schema.h"
#include "core/config/Session.h"

using namespace ttk;

namespace {

bool hasContent(const std::filesystem::path &path) {
    std::error_code code;

    return std::filesystem::is_regular_file(path, code) && std::filesystem::file_size(path, code) > 20;
}

bool blank(const std::filesystem::path &path) {
    std::ifstream stream(path, std::ios::binary);
    char letter = 0;

    while (stream.get(letter)) {
        if (std::isspace(static_cast<unsigned char>(letter)) == 0) {
            return false;
        }
    }

    return true;
}

std::filesystem::path firstExisting(const std::vector<std::filesystem::path> &paths) {
    for (const std::filesystem::path &path : paths) {
        if (hasContent(path)) {
            return path;
        }
    }

    return {};
}

// A .json on the command line is only this application's config if it says so. A
// mod's data file dropped on the binary would otherwise be opened as one and
// overwritten with ZDL's schema on the next save.
bool ourConfig(const std::filesystem::path &path) {
    std::error_code code;

    if (!std::filesystem::exists(path, code)) {
        return true;
    }

    const Json::Doc document = Json::read_file(path);

    return !document.valid() || Json::obj_get_string(document.root(), ConfigKey::ENGINE) == ConfigFile::ENGINE;
}

std::filesystem::path backupOf(const std::filesystem::path &path) {
    std::filesystem::path backup = path;

    return backup += ".bak";
}

// Only for a file that loaded cleanly.
void keepAsBackup(const std::filesystem::path &path) {
    std::error_code code;
    const std::filesystem::path backup = backupOf(path);

    std::filesystem::remove(backup, code);

    // A link keeps the loaded bytes, already on disk, because a save renames a new file over the path.
    std::filesystem::create_hard_link(path, backup, code);

    if (code) {
        std::filesystem::copy_file(path, backup, std::filesystem::copy_options::overwrite_existing, code);
    }
}

std::filesystem::path setAside(const std::filesystem::path &path, std::error_code &code) {
    std::filesystem::path aside = path;

    aside += ".bad";

    for (int suffix = 2; std::filesystem::exists(aside, code); suffix++) {
        aside = path;
        aside += ".bad-" + std::to_string(suffix);
    }

    std::filesystem::rename(path, aside, code);

    return aside;
}

bool sameBytes(const std::filesystem::path &one, const std::filesystem::path &other) {
    std::error_code code;

    if (std::filesystem::file_size(one, code) != std::filesystem::file_size(other, code) || code) {
        return false;
    }

    std::ifstream left(one, std::ios::binary);
    std::ifstream right(other, std::ios::binary);

    return std::equal(std::istreambuf_iterator<char>(left),
                      std::istreambuf_iterator<char>(),
                      std::istreambuf_iterator<char>(right),
                      std::istreambuf_iterator<char>());
}

bool writable(const std::filesystem::path &directory) {
    const std::filesystem::path probe = directory / (std::string(ConfigFile::JSON) + ".check");
    const bool opened = static_cast<bool>(std::ofstream(probe, std::ios::binary));
    std::error_code code;

    std::filesystem::remove(probe, code);

    return opened;
}

std::filesystem::path besideProgram() {
    return Paths::executable_directory() / ConfigFile::JSON;
}

// Where the user config already sits next to the program, there is no second place to move to.
bool portableApart() {
    return ConfigPaths::get().configPath(ConfigPaths::USER).parent_path() != Paths::executable_directory();
}

bool portableConfigBeside() {
    const std::filesystem::path directory = Paths::executable_directory();
    std::error_code code;

    return portableApart()
           && (std::filesystem::exists(directory / ConfigFile::JSON, code)
               || std::filesystem::exists(directory / ConfigFile::INI, code));
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
    const size_t length = std::strftime(written.data(), written.size(), "%Y-%m-%dT%H:%M:%SZ", &broken);

    return {written.data(), length};
}

// Without update_existing only absent folders are copied. Never moved, since a user config may
// share them.
void copyProfileFolders(const Config &config,
                        const std::filesystem::path &fromRoot,
                        const std::filesystem::path &toRoot,
                        const std::filesystem::copy_options options) {
    const std::filesystem::path from = fromRoot / ConfigFile::PROFILES_DIR;
    const bool absentOnly =
            (options & std::filesystem::copy_options::update_existing) == std::filesystem::copy_options::none;
    std::error_code code;

    if (fromRoot.empty() || toRoot.empty() || !std::filesystem::is_directory(from, code)) {
        return;
    }

    for (const Profile &profile : config.profiles) {
        const std::filesystem::path named(profile.config);

        if (named.empty() || named.is_absolute() || named.has_parent_path()) {
            continue;
        }

        const std::filesystem::path source = from / named.stem();
        const std::filesystem::path target = toRoot / ConfigFile::PROFILES_DIR / named.stem();

        if ((absentOnly && (std::filesystem::exists(target, code) || code))
            || !std::filesystem::is_directory(source, code)) {
            continue;
        }

        std::filesystem::create_directories(target.parent_path(), code);
        std::filesystem::copy(source, target, options, code);

        // Half a folder would stop the next start from trying again.
        if (code && absentOnly) {
            std::filesystem::remove_all(target, code);
        }
    }
}

// The saved config reads the user data folder, so the live saves and port configs go with it.
void leavePortable(const Config &config, const bool portable) {
    const std::filesystem::path root = ConfigPaths::dataDirectory();

    ConfigPaths::setPortable(false);

    if (portable) {
        copyProfileFolders(config,
                           root,
                           Paths::data_directory(),
                           std::filesystem::copy_options::recursive | std::filesystem::copy_options::update_existing);
    }
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
    const ConfigPaths &paths = ConfigPaths::get();

    return {paths.configPath(ConfigPaths::USER), firstExisting(paths.legacyConfigPath(ConfigPaths::USER))};
}

bool Session::read(const std::filesystem::path &jsonPath,
                   const std::filesystem::path &iniPath,
                   Config &into,
                   const bool migrate,
                   std::string *error) {
    std::error_code code;

    if (std::filesystem::exists(jsonPath, code)) {
        return into.load(jsonPath, error);
    }

    if (!iniPath.empty() && Import::loadLegacyFile(iniPath, into)) {
        if (migrate) {
            into.save(jsonPath);
        }

        return true;
    }

    return false;
}

bool Session::openedByHand() const {
    return _path != ConfigPaths::get().configPath(ConfigPaths::USER) && _path != besideProgram();
}

// Mirrors the choice start() makes.
bool Session::wantedOnDisk() const {
    const auto [json, ini] = userPaths();
    Config user;
    const Config *flags = nullptr;

    if (_path == json) {
        flags = &_config;
    } else if ((hasContent(json) || !ini.empty()) && read(json, ini, user, false)) {
        flags = &user;
    }

    if (flags != nullptr && !flags->general.noUserConf) {
        return false;
    }

    return (flags != nullptr && flags->general.moveAtNextStart) || portableConfigBeside();
}

bool Session::portableWanted() const {
    if (!_portableWanted) {
        _portableWanted = wantedOnDisk();
    }

    return *_portableWanted;
}

bool Session::setPortableWanted(const bool value, std::string *error) {
    if (value == portableWanted()) {
        return true;
    }

    std::string why;

    if (value && !portableApart()) {
        if (error != nullptr) {
            *error = "The user config already sits next to the program, so portable mode stays off";
        }

        return false;
    }

    if (value && !writable(Paths::executable_directory())) {
        if (error != nullptr) {
            *error = "ZDL4 cannot write to " + Paths::executable_directory().string() + ", so portable mode stays off";
        }

        return false;
    }

    const auto [json, ini] = userPaths();

    // Each flip toggles the move, so flipping back before a restart cancels it.
    if (_path == json) {
        const GeneralSettings before = _config.general;

        _config.general.noUserConf = value;
        _config.general.moveAtNextStart = !before.moveAtNextStart;

        if (!save(&why)) {
            _config.general = before;

            if (error != nullptr) {
                *error = "Could not write the user config: " + why;
            }

            return false;
        }
    } else {
        Config user;

        if (!read(json, ini, user, false, &why)) {
            std::error_code code;

            // A user config that does not parse is not written over.
            if (std::filesystem::exists(json, code)) {
                if (error != nullptr) {
                    *error = "Could not write the user config: could not read " + json.string()
                             + (why.empty() ? "" : ": " + why);
                }

                return false;
            }

            if (_source == Source::Portable) {
                user = _config;
                user.general.moveAtNextStart = false;
            }

            why.clear();
        }

        user.general.noUserConf = value;
        user.general.moveAtNextStart = !user.general.moveAtNextStart;

        if (!user.save(json, &why)) {
            if (error != nullptr) {
                *error = "Could not write the user config: " + why;
            }

            return false;
        }
    }

    _portableWanted = value;

    return true;
}

bool Session::moveToProgram(Config &user, const std::filesystem::path &userJson) {
    const std::filesystem::path target = besideProgram();
    std::filesystem::path staged = target;
    std::error_code code;

    staged += ".carry";
    user.general.moveAtNextStart = false;

    bool placed = portableApart() && user.save(staged);
    const bool replaced = placed && std::filesystem::exists(target, code);

    if (placed) {
        copyProfileFolders(user,
                           Paths::data_directory(),
                           Paths::executable_directory(),
                           std::filesystem::copy_options::recursive | std::filesystem::copy_options::update_existing);

        // The same bytes mean an earlier start stopped short, and the backup already holds the replaced one.
        if (Config old; replaced && !sameBytes(target, staged)) {
            if (old.load(target)) {
                keepAsBackup(target);
            } else if (!blank(target)) {
                setAside(target, code);
            }
        }

        std::filesystem::rename(staged, target, code);
        placed = !code;
    }

    if (!placed) {
        std::filesystem::remove(staged, code);

        user.general.noUserConf = false;
        _path = userJson;
        _source = Source::User;
        _config = std::move(user);
        _backUpOnSave = true;
        _portableWanted = false;
        _warning = "ZDL4 could not write to " + Paths::executable_directory().string()
                   + ", so portable mode is off and the user config stays in use.";

        save();

        return true;
    }

    // Last, so a start cut short moves the setup again.
    user.save(userJson);

    _path = target;
    _source = Source::Portable;
    _config = std::move(user);
    _backUpOnSave = !replaced;
    _portableWanted = true;

    return true;
}

bool Session::moveToUser(Config &user, const std::filesystem::path &userJson) {
    const std::filesystem::path directory = Paths::executable_directory();
    const std::filesystem::path portable = directory / ConfigFile::JSON;
    Config moved;
    std::string why;

    user.general.moveAtNextStart = false;

    if (!portableConfigBeside()) {
        return false;
    }

    if (!read(portable, directory / ConfigFile::INI, moved, false, &why)) {
        _warning = "Could not read " + portable.string() + (why.empty() ? "" : ": " + why)
                   + ". ZDL4 opened the user config as it was.";

        return false;
    }

    copyProfileFolders(moved,
                       directory,
                       Paths::data_directory(),
                       std::filesystem::copy_options::recursive | std::filesystem::copy_options::update_existing);

    moved.general.noUserConf = false;
    moved.general.moveAtNextStart = false;

    keepAsBackup(userJson);

    // The move is cleared in the same write, after the folders are in place.
    if (!moved.save(userJson, &why)) {
        std::error_code code;
        const bool json = std::filesystem::exists(portable, code);

        _path = portable;
        _legacy = json ? std::filesystem::path() : directory / ConfigFile::INI;
        _source = Source::Portable;
        _config = std::move(moved);
        _backUpOnSave = json;
        _portableWanted = false;
        _warning = "Could not write the user config at " + userJson.string() + (why.empty() ? "" : ": " + why)
                   + ". ZDL4 stays in portable mode for now.";

        return true;
    }

    _path = userJson;
    _source = Source::User;
    _config = std::move(moved);
    _backUpOnSave = false;
    _portableWanted = false;

    return true;
}

std::vector<std::string> Session::start(const std::vector<std::string> &arguments) {
    std::vector<std::string> rest;

    for (const std::string &argument : arguments) {
        if (Text::iends_with(argument, ConfigFile::JSON_EXT) && ourConfig(argument)) {
            _path = argument;
            _source = Source::UserSpecified;
        } else if (Text::iends_with(argument, ConfigFile::INI_EXT)) {
            _legacy = argument;
            _path = jsonSiblingOf(argument);
            _source = Source::UserSpecified;
        } else if (!argument.starts_with('-')) {
            rest.push_back(argument);

            continue;
        } else {
            continue;
        }

        break;
    }

    const ConfigPaths &paths = ConfigPaths::get();

    // The probe's read is reused when it turns out to be the config being opened.
    Config probed;
    bool reuseProbe = false;
    bool moved = false;

    if (_path.empty()) {
        const std::filesystem::path userJson = paths.configPath(ConfigPaths::USER);
        const std::filesystem::path userIni = firstExisting(paths.legacyConfigPath(ConfigPaths::USER));
        const bool json = hasContent(userJson);

        if (json || !userIni.empty()) {
            Config probe;

            if (read(userJson, userIni, probe, false)) {
                if (probe.general.moveAtNextStart) {
                    moved = probe.general.noUserConf ? moveToProgram(probe, userJson) : moveToUser(probe, userJson);
                }

                if (!moved && !probe.general.noUserConf) {
                    _path = userJson;
                    _legacy = userIni;
                    _source = Source::User;
                    reuseProbe = json;
                    probed = std::move(probe);
                }
            }
        }
    }

    // Portable mode: a config beside the executable, unless that is already the user location.
    if (_path.empty() && portableApart()) {
        const std::filesystem::path directory = Paths::executable_directory();
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
        _path = paths.configPath(ConfigPaths::USER);
        _legacy = firstExisting(paths.legacyConfigPath(ConfigPaths::USER));
        _source = Source::Fallback;
    }

    // Before the read, which looks for free profile folder names.
    ConfigPaths::setPortable(_source == Source::Portable);

    if (_source != Source::UserSpecified && !moved) {
        _portableWanted = _source == Source::Portable;
    }

    // Asked before the read, which writes the json when it migrates an ini.
    std::error_code code;
    const bool onDisk = !moved && std::filesystem::exists(_path, code);

    if (reuseProbe) {
        _config = std::move(probed);
        _backUpOnSave = true;
    } else if (std::string why; !moved && read(_path, _legacy, _config, true, &why)) {
        _backUpOnSave = onDisk;
    } else if (!moved) {
        recover(why);
    }

    // Before a .zdl adds a profile, which would otherwise be handed a folder meant for another.
    if (_source == Source::Portable && onDisk) {
        copyProfileFolders(_config,
                           Paths::data_directory(),
                           ConfigPaths::dataDirectory(),
                           std::filesystem::copy_options::recursive);
    }

    // A .zdl becomes its own profile. Loose files replace the active profile's list.
    bool replaceFiles = true;

    for (auto it = rest.begin(); it != rest.end();) {
        if (!Text::iends_with(*it, ConfigFile::ZDL_EXT)) {
            ++it;

            continue;
        }

        std::string id;

        if (Import::openZdlFile(_config, *it, id) != Import::ZdlOpen::Failed) {
            _config.ensureConfigFiles();
            _config.setActiveProfile(id);
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

const std::string &Session::warning() const {
    return _warning;
}

void Session::recover(const std::string &why) {
    std::error_code code;

    if (!std::filesystem::exists(_path, code)) {
        return;
    }

    // An empty file is no config yet, as when one is made beside the binary to go portable.
    const bool empty = blank(_path);
    const std::string unread = "Could not read " + _path.string() + (why.empty() ? "" : ": " + why) + ".";
    std::string kept;

    if (!empty) {
        const std::filesystem::path aside = setAside(_path, code);

        if (code) {
            _held = true;
            _warning = unread + " It could not be moved aside either, so nothing will be saved over it.";

            return;
        }

        kept = unread + " It was kept as " + aside.filename().string();
    }

    const std::filesystem::path backup = backupOf(_path);

    if (Config restored; restored.load(backup)) {
        _config = std::move(restored);
        _warning = (empty ? _path.string() + " was empty." : kept + ".") + " ZDL4 loaded the last good copy from "
                   + backup.filename().string() + ". Recent changes may be missing.";

        return;
    }

    if (!empty) {
        _warning = kept + ", and ZDL4 started with an empty config.";
    }
}

void Session::backUpOnce() {
    if (!_backUpOnSave) {
        return;
    }

    _backUpOnSave = false;

    keepAsBackup(_path);
}

bool Session::load(const std::filesystem::path &path, std::string *error) {
    Config loaded;

    if (Text::iends_with(path.string(), ConfigFile::INI_EXT)) {
        if (!Import::loadLegacyFile(path, loaded)) {
            if (error != nullptr) {
                *error = "could not read " + path.string();
            }

            return false;
        }

        _legacy = path;
        _path = jsonSiblingOf(path);
        _backUpOnSave = false;
    } else {
        if (!loaded.load(path, error)) {
            return false;
        }

        _legacy.clear();
        _path = path;
        _backUpOnSave = true;
    }

    _config = std::move(loaded);
    _source = Source::UserSpecified;
    ConfigPaths::setPortable(false);
    _held = false;
    _portableWanted.reset();

    return true;
}

bool Session::save(std::string *error) {
    if (_held) {
        if (error != nullptr) {
            *error = _path.string() + " could not be read, so it is left as it is";
        }

        return false;
    }

    backUpOnce();

    return _config.save(_path, error);
}

bool Session::saveAs(const std::filesystem::path &path, std::string *error) {
    if (path == _path) {
        backUpOnce();
    }

    if (!_config.save(path, error)) {
        return false;
    }

    _path = path;
    _legacy.clear();
    leavePortable(_config, _source == Source::Portable);
    _source = Source::UserSpecified;
    _held = false;
    _backUpOnSave = false;
    _portableWanted.reset();

    return true;
}

bool Session::adoptAsUserConfig(std::string *error) {
    const std::filesystem::path target = ConfigPaths::get().configPath(ConfigPaths::USER);
    const GeneralSettings before = _config.general;

    if (_held && target == _path) {
        return save(error);
    }

    if (target != _path) {
        _config.general.isImported = true;
        _config.general.importedFrom = _path.string();
        _config.general.importDate = nowInUtc();
    } else {
        backUpOnce();
    }

    // The next start opens it, as the confirmation promises.
    _config.general.noUserConf = false;
    _config.general.moveAtNextStart = false;

    if (!_config.save(target, error)) {
        _config.general = before;

        return false;
    }

    _path = target;
    _legacy.clear();
    leavePortable(_config, _source == Source::Portable);
    _source = Source::User;
    _held = false;
    _backUpOnSave = false;
    _portableWanted = false;

    return true;
}
