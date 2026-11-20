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
#include <string_view>
#include <type_traits>
#include <utility>

#include "ttk/system/Text.h"

#include "core/config/Config.h"
#include "core/config/ConfigPaths.h"
#include "core/config/NonDefault.h"
#include "core/config/Schema.h"
#include "core/launch/KnownPorts.h"
#include "core/ports/Catalog.h"

using namespace ttk;

namespace {

constexpr size_t CONFIG_STEM_LIMIT = 48;

bool sameZdlKey(const std::string &a, const std::string &b) {
    if (a.empty() || b.empty()) {
        return false;
    }

#ifdef _WIN32
    return Text::iequals(a, b);
#else
    return a == b;
#endif
}

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

bool folderTaken(const std::string &stem) {
    const std::filesystem::path folder = Config::profileFolder(stem);
    std::error_code asked;

    return !folder.empty() && std::filesystem::exists(folder, asked);
}

template <typename Entry>
Entry entryFromJson(yyjson_val *obj) {
    Entry entry;

    Json::each_field(obj, [&entry](const std::string_view key, const yyjson_val *val) {
        if (key == ConfigKey::NAME) {
            entry.name = Json::as_string(val);
        } else if (key == ConfigKey::FILE) {
            entry.file = Json::as_string(val);
        }

        if constexpr (std::is_same_v<Entry, Engine>) {
            if (key == ConfigKey::DOSBOX) {
                entry.dosbox = Json::as_bool(val);
            } else if (key == ConfigKey::PROGRAM) {
                entry.port = Port::fromId(Json::as_string(val));
            } else if (key == ConfigKey::MANAGED) {
                entry.managed = Json::as_bool(val);
            } else if (key == ConfigKey::DETECTED) {
                entry.detected = Json::as_bool(val);
            } else if (key == ConfigKey::PORT_VERSION) {
                entry.version = Json::as_string(val);
            }
        }
    });

    // A managed row cannot be edited, so only a port ZDL4 downloads may be managed.
    if constexpr (std::is_same_v<Entry, Engine>) {
        const Port::Port port = entry.port;

        entry.managed =
                entry.managed && std::ranges::any_of(Catalog::downloads(), [port](const Catalog::Download &download) {
                    return download.port == port;
                });
    }

    return entry;
}

template <typename Entry>
void readEntries(yyjson_val *arr, std::vector<Entry> &out) {
    out.clear();
    out.reserve(yyjson_arr_size(arr));

    Json::each_item(arr, [&out](yyjson_val *item) {
        auto entry = entryFromJson<Entry>(item);

        // A fileless entry can't be selected or launched.
        if (!entry.file.empty()) {
            out.push_back(std::move(entry));
        }
    });
}

template <typename Entry>
void writeEntries(const Json::Builder &builder,
                  yyjson_mut_val *root,
                  const char *key,
                  const std::vector<Entry> &entries) {
    const NonDefault put(builder);
    const Entry fresh;
    yyjson_mut_val *arr = builder.new_array();

    for (const Entry &entry : entries) {
        yyjson_mut_val *obj = builder.new_object();

        put.field(obj, ConfigKey::NAME, entry.name, fresh.name);
        put.field(obj, ConfigKey::FILE, entry.file, fresh.file);

        if constexpr (std::is_same_v<Entry, Engine>) {
            put.field(obj, ConfigKey::DOSBOX, entry.dosbox, fresh.dosbox);
            put.field(obj, ConfigKey::PROGRAM, Port::known(entry.port).id, std::string_view());
            put.field(obj, ConfigKey::MANAGED, entry.managed, fresh.managed);
            put.field(obj, ConfigKey::DETECTED, entry.detected, fresh.detected);
            put.field(obj, ConfigKey::PORT_VERSION, entry.version, fresh.version);
        }

        Json::Builder::append_value(arr, obj);
    }

    put.filled(root, key, arr);
}

void readLastDirs(yyjson_val *obj, LastDirs &dirs) {
    Json::each_field(obj, [&dirs](const std::string_view key, const yyjson_val *val) {
        if (key == ConfigKey::GENERAL) {
            dirs.general = Json::as_string(val);
        } else if (key == ConfigKey::WAD) {
            dirs.wad = Json::as_string(val);
        } else if (key == ConfigKey::SRC) {
            dirs.src = Json::as_string(val);
        } else if (key == ConfigKey::SAVE) {
            dirs.save = Json::as_string(val);
        } else if (key == ConfigKey::ZDL) {
            dirs.zdl = Json::as_string(val);
        } else if (key == ConfigKey::CONFIG) {
            dirs.config = Json::as_string(val);
        } else if (key == ConfigKey::REPLAY) {
            dirs.replay = Json::as_string(val);
        }
    });
}

void readWindow(yyjson_val *obj, WindowGeometry &window) {
    Json::each_field(obj, [&window](const std::string_view key, const yyjson_val *val) {
        int pair[2] = {0, 0};

        if (key == ConfigKey::SIZE && Json::as_int_array(val, pair, 2)) {
            window.hasSize = true;
            window.width = pair[0];
            window.height = pair[1];
        } else if (key == ConfigKey::POS && Json::as_int_array(val, pair, 2)) {
            window.hasPosition = true;
            window.x = pair[0];
            window.y = pair[1];
        } else if (key == ConfigKey::MAXIMIZED) {
            window.maximized = Json::as_bool(val);
        }
    });
}

void readGeneral(yyjson_val *obj, GeneralSettings &general) {
    Json::each_field(obj, [&general](const std::string_view key, yyjson_val *val) {
        if (key == ConfigKey::ALWAYS_ADD) {
            general.alwaysAdd = Json::as_string(val);
        } else if (key == ConfigKey::DOSBOX) {
            general.dosbox = Json::as_string(val);
        } else if (key == ConfigKey::AUTO_CLOSE) {
            general.autoClose = Json::as_bool(val);
        } else if (key == ConfigKey::LAUNCH_ZDL_IMMEDIATELY) {
            general.launchZdlImmediately = Json::as_bool(val);
        } else if (key == ConfigKey::SHOW_PATHS) {
            general.showPaths = Json::as_bool(val, general.showPaths);
        } else if (key == ConfigKey::NO_USER_CONF) {
            general.noUserConf = Json::as_bool(val);
        } else if (key == ConfigKey::MOVE_AT_NEXT_START) {
            general.moveAtNextStart = Json::as_bool(val);
        } else if (key == ConfigKey::SHOW_HIDDEN) {
            general.showHidden = Json::as_bool(val);
        } else if (key == ConfigKey::PROFILE_CONFIGS) {
            general.profileConfigs = Json::as_bool(val);
        } else if (key == ConfigKey::START_VIEW) {
            general.startView = Json::as_string(val, StartViewText::PROFILES) == StartViewText::GAMES
                                        ? StartView::Games
                                        : StartView::Profiles;
        } else if (key == ConfigKey::GAME_PORT) {
            general.gameEngine = Json::as_string(val);
        } else if (key == ConfigKey::THEME) {
            general.theme = Json::as_string(val, general.theme);
        } else if (key == ConfigKey::IS_IMPORTED) {
            general.isImported = Json::as_bool(val);
        } else if (key == ConfigKey::IMPORTED_FROM) {
            general.importedFrom = Json::as_string(val);
        } else if (key == ConfigKey::IMPORT_DATE) {
            general.importDate = Json::as_string(val);
        } else if (key == ConfigKey::LAST_DIRS) {
            readLastDirs(val, general.lastDirs);
        } else if (key == ConfigKey::WINDOW) {
            readWindow(val, general.window);
        }
    });
}

}

void Config::reset() {
    general = GeneralSettings();
    iwads.clear();
    engines.clear();
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
        return std::ranges::any_of(profiles,
                                   [&name](const Profile &profile) { return Text::iequals(profile.name, name); });
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

std::string Config::uniqueConfigFile(const std::string &name, const std::string &except) const {
    const std::string stem = fileNameFrom(name);
    const int excepted = except.empty() ? -1 : indexOfProfile(except);
    const std::string held = excepted < 0 ? std::string() : profiles[static_cast<size_t>(excepted)].config;

    const auto taken = [this, &held](const std::string &file) {
        if (Text::iequals(held, file)) {
            return false;
        }

        const bool mine = std::ranges::any_of(
                profiles, [&file](const Profile &profile) { return Text::iequals(profile.config, file); });

        // A deleted profile leaves its folder. Adopting one hands over what is in it.
        return mine || folderTaken(std::filesystem::path(file).stem().string());
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

std::filesystem::path Config::profileFolder(const std::string &stem) {
    const std::filesystem::path data = ConfigPaths::dataDirectory();

    return data.empty() ? std::filesystem::path() : data / ConfigFile::PROFILES_DIR / stem;
}

void Config::ensureConfigFiles() {
    for (Profile &profile : profiles) {
        if (profile.config.empty()) {
            profile.config = uniqueConfigFile(profile.name);
        }
    }
}

std::string Config::zdlKey(const std::filesystem::path &path) {
    std::error_code code;
    const std::filesystem::path absolute = std::filesystem::absolute(path, code);

    return (code ? path : absolute).lexically_normal().string();
}

std::string Config::profileOfZdl(const std::filesystem::path &path) const {
    const std::string key = zdlKey(path);

    for (const Profile &profile : profiles) {
        if (sameZdlKey(profile.zdlFile, key)) {
            return profile.id;
        }
    }

    return {};
}

void Config::claimZdl(const std::string &id, const std::filesystem::path &path) {
    const std::string key = zdlKey(path);

    for (Profile &profile : profiles) {
        if (profile.id == id) {
            profile.zdlFile = key;
        } else if (sameZdlKey(profile.zdlFile, key)) {
            profile.zdlFile.clear();
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
    copy.zdlFile.clear();
    copy.shortcut.clear();

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

    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization): id may be a reference into the profile erased below.
    const std::string removed = id;

    profiles.erase(profiles.begin() + index);

    if (activeProfileId != removed) {
        return;
    }

    activeProfileId = profiles.empty() ? std::string()
                                       : profiles[std::min<size_t>(static_cast<size_t>(index), profiles.size() - 1)].id;
}

const GameEntry *Config::findIwad(const std::string &name) const {
    for (const GameEntry &entry : iwads) {
        if (entry.name == name) {
            return &entry;
        }
    }

    return nullptr;
}

std::string Config::activeIwadFile() const {
    const GameEntry *game = findIwad(activeProfile().iwad);

    return game != nullptr ? game->file : std::string();
}

const Engine *Config::findEngine(const std::string &name) const {
    for (const Engine &engine : engines) {
        if (engine.name == name) {
            return &engine;
        }
    }

    return nullptr;
}

bool Config::load(const std::filesystem::path &path, std::string *error, Versions::Stamp *stamp) {
    const Json::Doc doc = Json::read_file(path, error);

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

    const int version = Versions::of(root);
    const bool older = version < Versions::CURRENT;
    const Json::Doc upgrade = older ? Versions::upgraded(doc, version) : Json::Doc();

    if (older && !upgrade.valid()) {
        if (error != nullptr) {
            *error = "out of memory bringing the file up to date";
        }

        return false;
    }

    reset();

    if (stamp != nullptr) {
        *stamp = {.version = version, .release = Json::obj_get_string(root, ConfigKey::APP_VERSION)};
    }

    Json::each_field(older ? upgrade.root() : root, [this](const std::string_view key, yyjson_val *val) {
        if (key == ConfigKey::GENERAL) {
            readGeneral(val, general);
        } else if (key == ConfigKey::IWADS) {
            readEntries(val, iwads);
        } else if (key == ConfigKey::PORTS) {
            readEntries(val, engines);
        } else if (key == ConfigKey::PROFILES) {
            profiles.reserve(yyjson_arr_size(val));

            Json::each_item(val, [this](yyjson_val *item) { profiles.push_back(Profile::fromJson(item)); });
        } else if (key == ConfigKey::ACTIVE_PROFILE) {
            activeProfileId = Json::as_string(val);
        }
    });

    ensureActiveProfile();
    ensureConfigFiles();

    return true;
}

bool Config::save(const std::filesystem::path &path, std::string *error) const {
    const Json::Builder builder;
    yyjson_mut_val *root = builder.new_object();

    builder.set_root(root);

    builder.add_int(root, ConfigKey::VERSION, Versions::CURRENT);
    builder.add_string(root, ConfigKey::ENGINE, ConfigFile::ENGINE);
    builder.add_string(root, ConfigKey::APP_VERSION, QZDL_VERSION);

    const NonDefault put(builder);
    const GeneralSettings fresh;
    yyjson_mut_val *gen = builder.new_object();

    put.field(gen, ConfigKey::ALWAYS_ADD, general.alwaysAdd, fresh.alwaysAdd);
    put.field(gen, ConfigKey::DOSBOX, general.dosbox, fresh.dosbox);
    put.field(gen, ConfigKey::AUTO_CLOSE, general.autoClose, fresh.autoClose);
    put.field(gen, ConfigKey::LAUNCH_ZDL_IMMEDIATELY, general.launchZdlImmediately, fresh.launchZdlImmediately);
    put.field(gen, ConfigKey::SHOW_PATHS, general.showPaths, fresh.showPaths);
    put.field(gen, ConfigKey::NO_USER_CONF, general.noUserConf, fresh.noUserConf);
    put.field(gen, ConfigKey::MOVE_AT_NEXT_START, general.moveAtNextStart, fresh.moveAtNextStart);
    put.field(gen, ConfigKey::SHOW_HIDDEN, general.showHidden, fresh.showHidden);
    put.field(gen, ConfigKey::PROFILE_CONFIGS, general.profileConfigs, fresh.profileConfigs);

    if (general.startView != fresh.startView) {
        builder.add_string(gen,
                           ConfigKey::START_VIEW,
                           general.startView == StartView::Games ? StartViewText::GAMES : StartViewText::PROFILES);
    }

    put.field(gen, ConfigKey::GAME_PORT, general.gameEngine, fresh.gameEngine);
    put.field(gen, ConfigKey::THEME, general.theme, fresh.theme);
    put.field(gen, ConfigKey::IS_IMPORTED, general.isImported, fresh.isImported);
    put.field(gen, ConfigKey::IMPORTED_FROM, general.importedFrom, fresh.importedFrom);
    put.field(gen, ConfigKey::IMPORT_DATE, general.importDate, fresh.importDate);

    yyjson_mut_val *window = builder.new_object();

    if (general.window.hasSize) {
        yyjson_mut_val *size = builder.new_array();

        builder.append_int(size, general.window.width);
        builder.append_int(size, general.window.height);
        builder.add_value(window, ConfigKey::SIZE, size);
    }

    if (general.window.hasPosition) {
        yyjson_mut_val *pos = builder.new_array();

        builder.append_int(pos, general.window.x);
        builder.append_int(pos, general.window.y);
        builder.add_value(window, ConfigKey::POS, pos);
    }

    put.field(window, ConfigKey::MAXIMIZED, general.window.maximized, fresh.window.maximized);
    put.filled(gen, ConfigKey::WINDOW, window);

    const LastDirs &last = general.lastDirs;
    yyjson_mut_val *dirs = builder.new_object();

    put.field(dirs, ConfigKey::GENERAL, last.general, fresh.lastDirs.general);
    put.field(dirs, ConfigKey::WAD, last.wad, fresh.lastDirs.wad);
    put.field(dirs, ConfigKey::SRC, last.src, fresh.lastDirs.src);
    put.field(dirs, ConfigKey::SAVE, last.save, fresh.lastDirs.save);
    put.field(dirs, ConfigKey::ZDL, last.zdl, fresh.lastDirs.zdl);
    put.field(dirs, ConfigKey::CONFIG, last.config, fresh.lastDirs.config);
    put.field(dirs, ConfigKey::REPLAY, last.replay, fresh.lastDirs.replay);
    put.filled(gen, ConfigKey::LAST_DIRS, dirs);

    put.filled(root, ConfigKey::GENERAL, gen);

    writeEntries(builder, root, ConfigKey::IWADS, iwads);
    writeEntries(builder, root, ConfigKey::PORTS, engines);

    put.field(root, ConfigKey::ACTIVE_PROFILE, activeProfileId, std::string());

    yyjson_mut_val *profileArr = builder.new_array();

    for (const Profile &profile : profiles) {
        Json::Builder::append_value(profileArr, profile.toJson(builder));
    }

    put.filled(root, ConfigKey::PROFILES, profileArr);

    return builder.write_file(path, error);
}
