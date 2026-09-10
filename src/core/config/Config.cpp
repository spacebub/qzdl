/*
 * This file is part of qZDL
 * Copyright (C) 2026  spacebub
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

#include "core/config/Config.h"
#include "core/util/Text.h"

namespace {

const char *UNNAMED_PROFILE = "New profile";
const char *CONFIG_FILE_SUFFIX = ".cfg";

constexpr const char *KEY_VERSION = "version";
constexpr const char *KEY_ENGINE = "engine";
constexpr const char *KEY_APP_VERSION = "appVersion";
constexpr const char *KEY_GENERAL = "general";
constexpr const char *KEY_IWADS = "iwads";
constexpr const char *KEY_PORTS = "ports";
constexpr const char *KEY_PROFILES = "profiles";
constexpr const char *KEY_ACTIVE_PROFILE = "activeProfile";

constexpr const char *KEY_ALWAYS_ADD = "alwaysAdd";
constexpr const char *KEY_DOSBOX = "dosbox";
constexpr const char *KEY_DETECTED = "detected";
constexpr const char *KEY_AUTO_CLOSE = "autoClose";
constexpr const char *KEY_LAUNCH_ZDL_IMMEDIATELY = "launchZdlImmediately";
constexpr const char *KEY_SHOW_PATHS = "showPaths";
constexpr const char *KEY_NO_USER_CONF = "noUserConf";
constexpr const char *KEY_SHOW_HIDDEN = "showHidden";
constexpr const char *KEY_PROFILE_CONFIGS = "profileConfigs";
constexpr const char *KEY_START_VIEW = "startView";
constexpr const char *KEY_GAME_PORT = "gamePort";
constexpr const char *KEY_THEME = "theme";
constexpr const char *KEY_IS_IMPORTED = "isImported";
constexpr const char *KEY_IMPORTED_FROM = "importedFrom";
constexpr const char *KEY_IMPORT_DATE = "importDate";

constexpr const char *KEY_WINDOW = "window";
constexpr const char *KEY_WINDOW_SIZE = "size";
constexpr const char *KEY_WINDOW_POS = "pos";

constexpr const char *KEY_LAST_DIRS = "lastDirs";
constexpr const char *KEY_DIR_GENERAL = "general";
constexpr const char *KEY_DIR_WAD = "wad";
constexpr const char *KEY_DIR_SRC = "src";
constexpr const char *KEY_DIR_SAVE = "save";
constexpr const char *KEY_DIR_ZDL = "zdl";
constexpr const char *KEY_DIR_CONFIG = "config";
constexpr const char *KEY_DIR_REPLAY = "replay";

constexpr const char *KEY_ENTRY_NAME = "name";
constexpr const char *KEY_ENTRY_FILE = "file";
constexpr const char *KEY_ENTRY_DOSBOX = "dosbox";

constexpr const char *ENGINE_NAME = "ZDL4";

constexpr size_t CONFIG_STEM_LIMIT = 48;

std::string fileNameFrom(const std::string &name) {
    std::string out;

    for (const char each : Text::lower(Text::trim(name))) {
        if ((each >= 'a' && each <= 'z') || (each >= '0' && each <= '9')) {
            out.push_back(each);
        } else if (!out.empty() && out.back() != '-') {
            out.push_back('-');
        }

        if (out.size() >= CONFIG_STEM_LIMIT) {
            break;
        }
    }

    while (!out.empty() && out.back() == '-') {
        out.pop_back();
    }

    return out.empty() ? std::string("profile") : out;
}

NameEntry entryFromJson(yyjson_val *obj) {
    return NameEntry{
        .name = Json::objGetString(obj, KEY_ENTRY_NAME),
        .file = Json::objGetString(obj, KEY_ENTRY_FILE),
        .dosbox = Json::objGetBool(obj, KEY_ENTRY_DOSBOX),
    };
}

void readEntries(yyjson_val *root, const char *key, std::vector<NameEntry> &out) {
    out.clear();

    yyjson_val *arr = Json::objGet(root, key);

    if (arr == nullptr || !yyjson_is_arr(arr)) {
        return;
    }

    size_t idx = 0;
    size_t max = 0;
    yyjson_val *item = nullptr;

    yyjson_arr_foreach(arr, idx, max, item) {
        NameEntry entry = entryFromJson(item);

        // A fileless entry can't be selected or launched.
        if (!entry.file.empty()) {
            out.push_back(std::move(entry));
        }
    }
}

void writeEntries(const Json::Builder &builder, yyjson_mut_val *root, const char *key,
                  const std::vector<NameEntry> &entries) {
    yyjson_mut_val *arr = builder.newArray();

    for (const NameEntry &entry : entries) {
        yyjson_mut_val *obj = builder.newObject();

        builder.addString(obj, KEY_ENTRY_NAME, entry.name);
        builder.addString(obj, KEY_ENTRY_FILE, entry.file);

        if (entry.dosbox) {
            builder.addBool(obj, KEY_ENTRY_DOSBOX, true);
        }

        Json::Builder::appendValue(arr, obj);
    }

    builder.addValue(root, key, arr);
}

void readLastDirs(yyjson_val *general, LastDirs &dirs) {
    yyjson_val *obj = Json::objGet(general, KEY_LAST_DIRS);

    dirs.general = Json::objGetString(obj, KEY_DIR_GENERAL);
    dirs.wad = Json::objGetString(obj, KEY_DIR_WAD);
    dirs.src = Json::objGetString(obj, KEY_DIR_SRC);
    dirs.save = Json::objGetString(obj, KEY_DIR_SAVE);
    dirs.zdl = Json::objGetString(obj, KEY_DIR_ZDL);
    dirs.config = Json::objGetString(obj, KEY_DIR_CONFIG);
    dirs.replay = Json::objGetString(obj, KEY_DIR_REPLAY);
}

}

void Config::reset() {
    general = GeneralSettings();
    iwads.clear();
    ports.clear();
    profiles.clear();
    activeProfileId.clear();
}

void Config::ensureActiveProfile() {
    if (indexOfProfile(activeProfileId) < 0) {
        activeProfileId = profiles.empty() ? std::string() : profiles.front().id;
    }
}

int Config::indexOfProfile(const std::string &id) const {
    if (id.empty()) {
        return -1;
    }

    for (size_t index = 0; index < profiles.size(); ++index) {
        if (profiles[index].id == id) {
            return static_cast<int>(index);
        }
    }

    return -1;
}

int Config::activeProfileIndex() const {
    if (profiles.empty()) {
        return -1;
    }

    const int index = indexOfProfile(activeProfileId);

    return index < 0 ? 0 : index;
}

Profile &Config::activeProfile() {
    return profiles.empty() ? _none : profiles[static_cast<size_t>(activeProfileIndex())];
}

const Profile &Config::activeProfile() const {
    return profiles.empty() ? _none : profiles[static_cast<size_t>(activeProfileIndex())];
}

bool Config::setActiveProfile(const std::string &id) {
    if (indexOfProfile(id) < 0) {
        return false;
    }

    activeProfileId = id;

    return true;
}

std::string Config::uniqueProfileName(const std::string &base) const {
    std::string candidate = Text::trim(base);

    if (candidate.empty()) {
        candidate = UNNAMED_PROFILE;
    }

    const auto taken = [this](const std::string &name) {
        return std::ranges::any_of(profiles, [&name](const Profile &profile) {
            return Text::iequals(profile.name, name);
        });
    };

    if (!taken(candidate)) {
        return candidate;
    }

    for (int suffix = 2;; suffix++) {
        std::string numbered = candidate + " (" + std::to_string(suffix) + ")";

        if (!taken(numbered)) {
            return numbered;
        }
    }
}

std::string Config::uniqueConfigFile(const std::string &name) const {
    const std::string stem = fileNameFrom(name);

    const auto taken = [this](const std::string &file) {
        return std::ranges::any_of(profiles, [&file](const Profile &profile) {
            return Text::iequals(profile.config, file);
        });
    };

    if (std::string candidate = stem + CONFIG_FILE_SUFFIX; !taken(candidate)) {
        return candidate;
    }

    for (int suffix = 2;; suffix++) {
        std::string numbered = stem + "-" + std::to_string(suffix) + CONFIG_FILE_SUFFIX;

        if (!taken(numbered)) {
            return numbered;
        }
    }
}

void Config::ensureConfigFiles() {
    for (Profile &profile : profiles) {
        if (profile.config.empty()) {
            profile.config = uniqueConfigFile(profile.name);
        }
    }
}

std::string Config::addProfile(const std::string &name) {
    Profile profile;

    profile.id = Profile::newId();
    profile.name = uniqueProfileName(name);
    profile.config = uniqueConfigFile(profile.name);

    std::string id = profile.id;
    profiles.push_back(std::move(profile));

    return id;
}

std::string Config::duplicateActiveProfile(const std::string &name) {
    Profile copy = activeProfile();

    copy.id = Profile::newId();
    copy.name = uniqueProfileName(name);

    copy.config = uniqueConfigFile(copy.name);

    std::string id = copy.id;
    profiles.push_back(std::move(copy));

    return id;
}

void Config::removeProfile(const std::string &id) {
    const int index = indexOfProfile(id);

    if (index < 0) {
        return;
    }

    profiles.erase(profiles.begin() + index);

    if (activeProfileId != id) {
        return;
    }

    activeProfileId = profiles.empty()
        ? std::string()
        : profiles[std::min<size_t>(static_cast<size_t>(index), profiles.size() - 1)].id;
}

const NameEntry *Config::findIwad(const std::string &name) const {
    for (const NameEntry &entry : iwads) {
        if (entry.name == name) {
            return &entry;
        }
    }

    return nullptr;
}

std::string Config::activeIwadFile() const {
    const NameEntry *game = findIwad(activeProfile().iwad);

    return game != nullptr ? game->file : std::string();
}

const NameEntry *Config::findPort(const std::string &name) const {
    for (const NameEntry &entry : ports) {
        if (entry.name == name) {
            return &entry;
        }
    }

    return nullptr;
}

bool Config::load(const std::filesystem::path &path, std::string *error) {
    const Json::Doc doc = Json::readFile(path, error);

    if (!doc.valid()) {
        return false;
    }

    yyjson_val *root = doc.root();

    if (root == nullptr || !yyjson_is_obj(root)) {
        if (error != nullptr) {
            *error = "root value is not an object";
        }

        return false;
    }

    reset();

    yyjson_val *gen = Json::objGet(root, KEY_GENERAL);

    general.alwaysAdd = Json::objGetString(gen, KEY_ALWAYS_ADD);
    general.dosbox = Json::objGetString(gen, KEY_DOSBOX);
    general.detected = Json::objGetStringList(gen, KEY_DETECTED);
    general.autoClose = Json::objGetBool(gen, KEY_AUTO_CLOSE);
    general.launchZdlImmediately = Json::objGetBool(gen, KEY_LAUNCH_ZDL_IMMEDIATELY);
    general.showPaths = Json::objGetBool(gen, KEY_SHOW_PATHS, ConfigDefaults::SHOW_PATHS);
    general.noUserConf = Json::objGetBool(gen, KEY_NO_USER_CONF);
    general.showHidden = Json::objGetBool(gen, KEY_SHOW_HIDDEN);
    general.profileConfigs = Json::objGetBool(gen, KEY_PROFILE_CONFIGS);
    general.startView = Json::objGetString(gen, KEY_START_VIEW, ConfigDefaults::START_VIEW);
    general.gamePort = Json::objGetString(gen, KEY_GAME_PORT);
    general.theme = Json::objGetString(gen, KEY_THEME, ConfigDefaults::THEME);
    general.isImported = Json::objGetBool(gen, KEY_IS_IMPORTED);
    general.importedFrom = Json::objGetString(gen, KEY_IMPORTED_FROM);
    general.importDate = Json::objGetString(gen, KEY_IMPORT_DATE);
    readLastDirs(gen, general.lastDirs);

    yyjson_val *window = Json::objGet(gen, KEY_WINDOW);
    int pair[2] = {0, 0};

    if (Json::objGetIntArray(window, KEY_WINDOW_SIZE, pair, 2)) {
        general.window.hasSize = true;
        general.window.width = pair[0];
        general.window.height = pair[1];
    }

    if (Json::objGetIntArray(window, KEY_WINDOW_POS, pair, 2)) {
        general.window.hasPosition = true;
        general.window.x = pair[0];
        general.window.y = pair[1];
    }

    readEntries(root, KEY_IWADS, iwads);
    readEntries(root, KEY_PORTS, ports);

    yyjson_val *profileArr = Json::objGet(root, KEY_PROFILES);

    if (profileArr != nullptr && yyjson_is_arr(profileArr)) {
        size_t idx = 0;
        size_t max = 0;
        yyjson_val *item = nullptr;

        yyjson_arr_foreach(profileArr, idx, max, item) {
            profiles.push_back(Profile::fromJson(item));
        }
    }

    activeProfileId = Json::objGetString(root, KEY_ACTIVE_PROFILE);
    ensureActiveProfile();
    ensureConfigFiles();

    return true;
}

bool Config::save(const std::filesystem::path &path, std::string *error) const {
    const Json::Builder builder;
    yyjson_mut_val *root = builder.newObject();

    builder.setRoot(root);

    builder.addInt(root, KEY_VERSION, SCHEMA_VERSION);
    builder.addString(root, KEY_ENGINE, ENGINE_NAME);
    builder.addString(root, KEY_APP_VERSION, QZDL_VERSION);

    yyjson_mut_val *gen = builder.newObject();

    builder.addString(gen, KEY_ALWAYS_ADD, general.alwaysAdd);
    builder.addString(gen, KEY_DOSBOX, general.dosbox);
    builder.addBool(gen, KEY_AUTO_CLOSE, general.autoClose);
    builder.addBool(gen, KEY_LAUNCH_ZDL_IMMEDIATELY, general.launchZdlImmediately);
    builder.addBool(gen, KEY_SHOW_PATHS, general.showPaths);
    builder.addBool(gen, KEY_NO_USER_CONF, general.noUserConf);
    builder.addBool(gen, KEY_SHOW_HIDDEN, general.showHidden);
    builder.addBool(gen, KEY_PROFILE_CONFIGS, general.profileConfigs);
    builder.addString(gen, KEY_START_VIEW, general.startView);
    builder.addString(gen, KEY_GAME_PORT, general.gamePort);
    builder.addString(gen, KEY_THEME, general.theme);
    builder.addBool(gen, KEY_IS_IMPORTED, general.isImported);
    builder.addString(gen, KEY_IMPORTED_FROM, general.importedFrom);
    builder.addString(gen, KEY_IMPORT_DATE, general.importDate);

    yyjson_mut_val *window = builder.newObject();

    if (general.window.hasSize) {
        yyjson_mut_val *size = builder.newArray();

        builder.appendInt(size, general.window.width);
        builder.appendInt(size, general.window.height);
        builder.addValue(window, KEY_WINDOW_SIZE, size);
    }

    if (general.window.hasPosition) {
        yyjson_mut_val *pos = builder.newArray();

        builder.appendInt(pos, general.window.x);
        builder.appendInt(pos, general.window.y);
        builder.addValue(window, KEY_WINDOW_POS, pos);
    }

    builder.addValue(gen, KEY_WINDOW, window);

    yyjson_mut_val *dirs = builder.newObject();

    builder.addString(dirs, KEY_DIR_GENERAL, general.lastDirs.general);
    builder.addString(dirs, KEY_DIR_WAD, general.lastDirs.wad);
    builder.addString(dirs, KEY_DIR_SRC, general.lastDirs.src);
    builder.addString(dirs, KEY_DIR_SAVE, general.lastDirs.save);
    builder.addString(dirs, KEY_DIR_ZDL, general.lastDirs.zdl);
    builder.addString(dirs, KEY_DIR_CONFIG, general.lastDirs.config);
    builder.addString(dirs, KEY_DIR_REPLAY, general.lastDirs.replay);
    builder.addValue(gen, KEY_LAST_DIRS, dirs);

    yyjson_mut_val *detected = builder.newArray();

    for (const std::string &file : general.detected) {
        builder.appendString(detected, file);
    }

    builder.addValue(gen, KEY_DETECTED, detected);

    builder.addValue(root, KEY_GENERAL, gen);

    writeEntries(builder, root, KEY_IWADS, iwads);
    writeEntries(builder, root, KEY_PORTS, ports);

    builder.addString(root, KEY_ACTIVE_PROFILE, activeProfileId);

    yyjson_mut_val *profileArr = builder.newArray();

    for (const Profile &profile : profiles) {
        Json::Builder::appendValue(profileArr, profile.toJson(builder));
    }

    builder.addValue(root, KEY_PROFILES, profileArr);

    return builder.writeFile(path, error);
}
