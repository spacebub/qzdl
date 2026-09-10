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
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "core/config/Profile.h"

namespace ConfigDefaults {
inline constexpr bool SHOW_PATHS = true;
inline constexpr const char *START_VIEW = "profiles";
inline constexpr const char *THEME = "system";
}

struct NameEntry {
    std::string name;
    std::string file;

    // Ports only.
    bool dosbox{false};
};

struct LastDirs {
    std::string general;
    std::string wad;
    std::string src;
    std::string save;
    std::string zdl;
    std::string config;
    std::string replay;
};

struct WindowGeometry {
    int width{0};
    int height{0};
    int x{0};
    int y{0};
    bool hasSize{false};
    bool hasPosition{false};
};

struct GeneralSettings {
    std::string alwaysAdd;

    std::string dosbox;

    // Port ids detection has already offered, so removed ones are not re-added.
    std::vector<std::string> detected;

    bool autoClose{false};
    bool launchZdlImmediately{false};
    bool showPaths{ConfigDefaults::SHOW_PATHS};
    bool noUserConf{false};

    bool showHidden{false};

    bool profileConfigs{false};

    std::string startView{ConfigDefaults::START_VIEW};

    // Port for games launched from the library; empty uses the open profile's.
    std::string gamePort;

    std::string theme{ConfigDefaults::THEME};

    bool isImported{false};
    std::string importedFrom;
    std::string importDate;

    WindowGeometry window;
    LastDirs lastDirs;
};

class Config {
public:
    static constexpr int SCHEMA_VERSION = 1;

    bool load(const std::filesystem::path &path, std::string *error = nullptr);

    bool save(const std::filesystem::path &path, std::string *error = nullptr) const;

    void reset();

    GeneralSettings general;
    std::vector<NameEntry> iwads;
    std::vector<NameEntry> ports;
    std::vector<Profile> profiles;
    std::string activeProfileId;

    Profile &activeProfile();

    [[nodiscard]] const Profile &activeProfile() const;

    [[nodiscard]] int activeProfileIndex() const;

    [[nodiscard]] int indexOfProfile(const std::string &id) const;

    bool setActiveProfile(const std::string &id);

    std::string addProfile(const std::string &name);

    std::string duplicateActiveProfile(const std::string &name);

    void removeProfile(const std::string &id);

    void ensureActiveProfile();

    [[nodiscard]] std::string uniqueProfileName(const std::string &base) const;
    [[nodiscard]] std::string uniqueConfigFile(const std::string &name) const;

    void ensureConfigFiles();

    [[nodiscard]] const NameEntry *findIwad(const std::string &name) const;

    // Empty when the active profile has no game.
    [[nodiscard]] std::string activeIwadFile() const;

    [[nodiscard]] const NameEntry *findPort(const std::string &name) const;

private:
    // Returned by activeProfile() when there are no profiles; never saved.
    mutable Profile _none;
};
