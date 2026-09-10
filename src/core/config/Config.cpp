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
#include <utility>

#include "core/config/Config.h"
#include "core/config/Schema.h"
#include "core/util/Text.h"

namespace {

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

    return out.empty() ? std::string(ConfigFile::PROFILE_STEM) : out;
}

NameEntry entryFromJson(yyjson_val *obj) {
    return NameEntry{
        .name = Json::objGetString(obj, ConfigKey::NAME),
        .file = Json::objGetString(obj, ConfigKey::FILE),
        .dosbox = Json::objGetBool(obj, ConfigKey::DOSBOX),
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

        builder.addString(obj, ConfigKey::NAME, entry.name);
        builder.addString(obj, ConfigKey::FILE, entry.file);

        if (entry.dosbox) {
            builder.addBool(obj, ConfigKey::DOSBOX, true);
        }

        Json::Builder::appendValue(arr, obj);
    }

    builder.addValue(root, key, arr);
}

void readLastDirs(yyjson_val *general, LastDirs &dirs) {
    yyjson_val *obj = Json::objGet(general, ConfigKey::LAST_DIRS);

    dirs.general = Json::objGetString(obj, ConfigKey::GENERAL);
    dirs.wad = Json::objGetString(obj, ConfigKey::WAD);
    dirs.src = Json::objGetString(obj, ConfigKey::SRC);
    dirs.save = Json::objGetString(obj, ConfigKey::SAVE);
    dirs.zdl = Json::objGetString(obj, ConfigKey::ZDL);
    dirs.config = Json::objGetString(obj, ConfigKey::CONFIG);
    dirs.replay = Json::objGetString(obj, ConfigKey::REPLAY);
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
        candidate = ProfileName::UNNAMED;
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

    if (std::string candidate = stem + ConfigFile::CFG_EXT; !taken(candidate)) {
        return candidate;
    }

    for (int suffix = 2;; suffix++) {
        std::string numbered = stem + "-" + std::to_string(suffix) + ConfigFile::CFG_EXT;

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

    yyjson_val *gen = Json::objGet(root, ConfigKey::GENERAL);

    general.alwaysAdd = Json::objGetString(gen, ConfigKey::ALWAYS_ADD);
    general.dosbox = Json::objGetString(gen, ConfigKey::DOSBOX);
    general.detected = Json::objGetStringList(gen, ConfigKey::DETECTED);
    general.autoClose = Json::objGetBool(gen, ConfigKey::AUTO_CLOSE);
    general.launchZdlImmediately = Json::objGetBool(gen, ConfigKey::LAUNCH_ZDL_IMMEDIATELY);
    general.showPaths = Json::objGetBool(gen, ConfigKey::SHOW_PATHS, ConfigDefaults::SHOW_PATHS);
    general.noUserConf = Json::objGetBool(gen, ConfigKey::NO_USER_CONF);
    general.showHidden = Json::objGetBool(gen, ConfigKey::SHOW_HIDDEN);
    general.profileConfigs = Json::objGetBool(gen, ConfigKey::PROFILE_CONFIGS);
    general.startView = Json::objGetString(gen, ConfigKey::START_VIEW, ConfigDefaults::START_VIEW);
    general.gamePort = Json::objGetString(gen, ConfigKey::GAME_PORT);
    general.theme = Json::objGetString(gen, ConfigKey::THEME, ConfigDefaults::THEME);
    general.isImported = Json::objGetBool(gen, ConfigKey::IS_IMPORTED);
    general.importedFrom = Json::objGetString(gen, ConfigKey::IMPORTED_FROM);
    general.importDate = Json::objGetString(gen, ConfigKey::IMPORT_DATE);
    readLastDirs(gen, general.lastDirs);

    yyjson_val *window = Json::objGet(gen, ConfigKey::WINDOW);
    int pair[2] = {0, 0};

    if (Json::objGetIntArray(window, ConfigKey::SIZE, pair, 2)) {
        general.window.hasSize = true;
        general.window.width = pair[0];
        general.window.height = pair[1];
    }

    if (Json::objGetIntArray(window, ConfigKey::POS, pair, 2)) {
        general.window.hasPosition = true;
        general.window.x = pair[0];
        general.window.y = pair[1];
    }

    readEntries(root, ConfigKey::IWADS, iwads);
    readEntries(root, ConfigKey::PORTS, ports);

    yyjson_val *profileArr = Json::objGet(root, ConfigKey::PROFILES);

    if (profileArr != nullptr && yyjson_is_arr(profileArr)) {
        size_t idx = 0;
        size_t max = 0;
        yyjson_val *item = nullptr;

        yyjson_arr_foreach(profileArr, idx, max, item) {
            profiles.push_back(Profile::fromJson(item));
        }
    }

    activeProfileId = Json::objGetString(root, ConfigKey::ACTIVE_PROFILE);
    ensureActiveProfile();
    ensureConfigFiles();

    return true;
}

bool Config::save(const std::filesystem::path &path, std::string *error) const {
    const Json::Builder builder;
    yyjson_mut_val *root = builder.newObject();

    builder.setRoot(root);

    builder.addInt(root, ConfigKey::VERSION, SCHEMA_VERSION);
    builder.addString(root, ConfigKey::ENGINE, ConfigFile::ENGINE);
    builder.addString(root, ConfigKey::APP_VERSION, QZDL_VERSION);

    yyjson_mut_val *gen = builder.newObject();

    builder.addString(gen, ConfigKey::ALWAYS_ADD, general.alwaysAdd);
    builder.addString(gen, ConfigKey::DOSBOX, general.dosbox);
    builder.addBool(gen, ConfigKey::AUTO_CLOSE, general.autoClose);
    builder.addBool(gen, ConfigKey::LAUNCH_ZDL_IMMEDIATELY, general.launchZdlImmediately);
    builder.addBool(gen, ConfigKey::SHOW_PATHS, general.showPaths);
    builder.addBool(gen, ConfigKey::NO_USER_CONF, general.noUserConf);
    builder.addBool(gen, ConfigKey::SHOW_HIDDEN, general.showHidden);
    builder.addBool(gen, ConfigKey::PROFILE_CONFIGS, general.profileConfigs);
    builder.addString(gen, ConfigKey::START_VIEW, general.startView);
    builder.addString(gen, ConfigKey::GAME_PORT, general.gamePort);
    builder.addString(gen, ConfigKey::THEME, general.theme);
    builder.addBool(gen, ConfigKey::IS_IMPORTED, general.isImported);
    builder.addString(gen, ConfigKey::IMPORTED_FROM, general.importedFrom);
    builder.addString(gen, ConfigKey::IMPORT_DATE, general.importDate);

    yyjson_mut_val *window = builder.newObject();

    if (general.window.hasSize) {
        yyjson_mut_val *size = builder.newArray();

        builder.appendInt(size, general.window.width);
        builder.appendInt(size, general.window.height);
        builder.addValue(window, ConfigKey::SIZE, size);
    }

    if (general.window.hasPosition) {
        yyjson_mut_val *pos = builder.newArray();

        builder.appendInt(pos, general.window.x);
        builder.appendInt(pos, general.window.y);
        builder.addValue(window, ConfigKey::POS, pos);
    }

    builder.addValue(gen, ConfigKey::WINDOW, window);

    yyjson_mut_val *dirs = builder.newObject();

    builder.addString(dirs, ConfigKey::GENERAL, general.lastDirs.general);
    builder.addString(dirs, ConfigKey::WAD, general.lastDirs.wad);
    builder.addString(dirs, ConfigKey::SRC, general.lastDirs.src);
    builder.addString(dirs, ConfigKey::SAVE, general.lastDirs.save);
    builder.addString(dirs, ConfigKey::ZDL, general.lastDirs.zdl);
    builder.addString(dirs, ConfigKey::CONFIG, general.lastDirs.config);
    builder.addString(dirs, ConfigKey::REPLAY, general.lastDirs.replay);
    builder.addValue(gen, ConfigKey::LAST_DIRS, dirs);

    yyjson_mut_val *detected = builder.newArray();

    for (const std::string &file : general.detected) {
        builder.appendString(detected, file);
    }

    builder.addValue(gen, ConfigKey::DETECTED, detected);

    builder.addValue(root, ConfigKey::GENERAL, gen);

    writeEntries(builder, root, ConfigKey::IWADS, iwads);
    writeEntries(builder, root, ConfigKey::PORTS, ports);

    builder.addString(root, ConfigKey::ACTIVE_PROFILE, activeProfileId);

    yyjson_mut_val *profileArr = builder.newArray();

    for (const Profile &profile : profiles) {
        Json::Builder::appendValue(profileArr, profile.toJson(builder));
    }

    builder.addValue(root, ConfigKey::PROFILES, profileArr);

    return builder.writeFile(path, error);
}
