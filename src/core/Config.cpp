/*
 * This file is part of qZDL
 * Copyright (C) 2026  spacebub
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

#include <algorithm>

#include "core/Config.h"
#include "core/Text.h"

namespace {

const char *DEFAULT_PROFILE_NAME = "Default";
const char *CONFIG_FILE_SUFFIX = ".cfg";

// Long enough to still read as the profile it belongs to, short enough that no
// file system minds it however deep the data directory sits.
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
        .name = Json::objGetString(obj, "name"),
        .file = Json::objGetString(obj, "file"),
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

        builder.addString(obj, "name", entry.name);
        builder.addString(obj, "file", entry.file);
        Json::Builder::appendValue(arr, obj);
    }

    builder.addValue(root, key, arr);
}

void readLastDirs(yyjson_val *general, LastDirs &dirs) {
    yyjson_val *obj = Json::objGet(general, "lastDirs");

    dirs.general = Json::objGetString(obj, "general");
    dirs.wad = Json::objGetString(obj, "wad");
    dirs.src = Json::objGetString(obj, "src");
    dirs.save = Json::objGetString(obj, "save");
    dirs.zdl = Json::objGetString(obj, "zdl");
    dirs.config = Json::objGetString(obj, "config");
}

}

Config::Config() {
    ensureProfile();
}

void Config::clear() {
    general = GeneralSettings();
    iwads.clear();
    ports.clear();
    profiles.clear();
    activeProfileId.clear();
    ensureProfile();
}

void Config::ensureProfile() {
    if (profiles.empty()) {
        Profile profile;

        profile.id = Profile::newId();
        profile.name = DEFAULT_PROFILE_NAME;
        profile.config = uniqueConfigFile(profile.name);
        profiles.push_back(std::move(profile));
    }

    if (indexOfProfile(activeProfileId) < 0) {
        activeProfileId = profiles.front().id;
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
    const int index = indexOfProfile(activeProfileId);

    return index < 0 ? 0 : index;
}

Profile &Config::activeProfile() {
    ensureProfile();

    return profiles[static_cast<size_t>(activeProfileIndex())];
}

const Profile &Config::activeProfile() const {
    return profiles[static_cast<size_t>(activeProfileIndex())];
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
        candidate = DEFAULT_PROFILE_NAME;
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

    // A copy is a separate profile, so it starts on settings of its own rather
    // than sharing the config file of the profile it was copied from.
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

    // Never leave the user with no profile at all; empty the last one instead.
    if (profiles.size() == 1) {
        profiles[0].clearSettings();
        profiles[0].name = DEFAULT_PROFILE_NAME;
        activeProfileId = profiles[0].id;

        return;
    }

    profiles.erase(profiles.begin() + index);

    if (activeProfileId == id) {
        activeProfileId = profiles[std::min<size_t>(static_cast<size_t>(index), profiles.size() - 1)].id;
    }
}

const NameEntry *Config::findIwad(const std::string &name) const {
    for (const NameEntry &entry : iwads) {
        if (entry.name == name) {
            return &entry;
        }
    }

    return nullptr;
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

    clear();
    profiles.clear();

    yyjson_val *gen = Json::objGet(root, "general");

    general.alwaysAdd = Json::objGetString(gen, "alwaysAdd");
    general.autoClose = Json::objGetBool(gen, "autoClose");
    general.launchZdlImmediately = Json::objGetBool(gen, "launchZdlImmediately");
    general.rememberFileList = Json::objGetBool(gen, "rememberFileList", true);
    general.showPaths = Json::objGetBool(gen, "showPaths", true);
    general.noUserConf = Json::objGetBool(gen, "noUserConf");
    general.profileConfigs = Json::objGetBool(gen, "profileConfigs");
    general.theme = Json::objGetString(gen, "theme", "system");
    general.isImported = Json::objGetBool(gen, "isImported");
    general.doNotImportThis = Json::objGetBool(gen, "doNotImportThis");
    general.importedFrom = Json::objGetString(gen, "importedFrom");
    general.importDate = Json::objGetString(gen, "importDate");
    readLastDirs(gen, general.lastDirs);

    yyjson_val *window = Json::objGet(gen, "window");
    int pair[2] = {0, 0};

    if (Json::objGetIntArray(window, "size", pair, 2)) {
        general.window.hasSize = true;
        general.window.width = pair[0];
        general.window.height = pair[1];
    }

    if (Json::objGetIntArray(window, "pos", pair, 2)) {
        general.window.hasPosition = true;
        general.window.x = pair[0];
        general.window.y = pair[1];
    }

    readEntries(root, "iwads", iwads);
    readEntries(root, "ports", ports);

    yyjson_val *profileArr = Json::objGet(root, "profiles");

    if (profileArr != nullptr && yyjson_is_arr(profileArr)) {
        size_t idx = 0;
        size_t max = 0;
        yyjson_val *item = nullptr;

        yyjson_arr_foreach(profileArr, idx, max, item) {
            profiles.push_back(Profile::fromJson(item));
        }
    }

    activeProfileId = Json::objGetString(root, "activeProfile");
    ensureProfile();
    ensureConfigFiles();

    return true;
}

bool Config::save(const std::filesystem::path &path, std::string *error) const {
    const Json::Builder builder;
    yyjson_mut_val *root = builder.newObject();

    builder.setRoot(root);

    builder.addInt(root, "version", SCHEMA_VERSION);
    builder.addString(root, "engine", "ZDL");
    builder.addString(root, "appVersion", QZDL_VERSION);

    yyjson_mut_val *gen = builder.newObject();

    builder.addString(gen, "alwaysAdd", general.alwaysAdd);
    builder.addBool(gen, "autoClose", general.autoClose);
    builder.addBool(gen, "launchZdlImmediately", general.launchZdlImmediately);
    builder.addBool(gen, "rememberFileList", general.rememberFileList);
    builder.addBool(gen, "showPaths", general.showPaths);
    builder.addBool(gen, "noUserConf", general.noUserConf);
    builder.addBool(gen, "profileConfigs", general.profileConfigs);
    builder.addString(gen, "theme", general.theme);
    builder.addBool(gen, "isImported", general.isImported);
    builder.addBool(gen, "doNotImportThis", general.doNotImportThis);
    builder.addString(gen, "importedFrom", general.importedFrom);
    builder.addString(gen, "importDate", general.importDate);

    yyjson_mut_val *window = builder.newObject();

    if (general.window.hasSize) {
        yyjson_mut_val *size = builder.newArray();

        builder.appendInt(size, general.window.width);
        builder.appendInt(size, general.window.height);
        builder.addValue(window, "size", size);
    }

    if (general.window.hasPosition) {
        yyjson_mut_val *pos = builder.newArray();

        builder.appendInt(pos, general.window.x);
        builder.appendInt(pos, general.window.y);
        builder.addValue(window, "pos", pos);
    }

    builder.addValue(gen, "window", window);

    yyjson_mut_val *dirs = builder.newObject();

    builder.addString(dirs, "general", general.lastDirs.general);
    builder.addString(dirs, "wad", general.lastDirs.wad);
    builder.addString(dirs, "src", general.lastDirs.src);
    builder.addString(dirs, "save", general.lastDirs.save);
    builder.addString(dirs, "zdl", general.lastDirs.zdl);
    builder.addString(dirs, "config", general.lastDirs.config);
    builder.addValue(gen, "lastDirs", dirs);

    builder.addValue(root, "general", gen);

    writeEntries(builder, root, "iwads", iwads);
    writeEntries(builder, root, "ports", ports);

    builder.addString(root, "activeProfile", activeProfileId);

    yyjson_mut_val *profileArr = builder.newArray();

    for (const Profile &profile : profiles) {
        Json::Builder::appendValue(profileArr, profile.toJson(builder));
    }

    builder.addValue(root, "profiles", profileArr);

    return builder.writeFile(path, error);
}
