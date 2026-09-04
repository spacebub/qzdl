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
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "core/Profile.h"

struct NameEntry {
    std::string name;
    std::string file;
};

struct LastDirs {
    std::string general;
    std::string wad;
    std::string src;
    std::string save;
    std::string zdl;
    std::string config;
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
    bool autoClose{false};
    bool launchZdlImmediately{false};
    bool rememberFileList{true};
    bool showPaths{true};
    bool noUserConf{false};

    bool profileConfigs{false};

    std::string theme{"system"};

    bool isImported{false};
    bool doNotImportThis{false};
    std::string importedFrom;
    std::string importDate;

    WindowGeometry window;
    LastDirs lastDirs;
};

class Config {
public:
    Config();

    static constexpr int SCHEMA_VERSION = 1;

    bool load(const std::filesystem::path &path, std::string *error = nullptr);

    bool save(const std::filesystem::path &path, std::string *error = nullptr) const;

    void clear();

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

    void ensureProfile();

    [[nodiscard]] std::string uniqueProfileName(const std::string &base) const;
    [[nodiscard]] std::string uniqueConfigFile(const std::string &name) const;

    void ensureConfigFiles();

    [[nodiscard]] const NameEntry *findIwad(const std::string &name) const;

    [[nodiscard]] const NameEntry *findPort(const std::string &name) const;
};
