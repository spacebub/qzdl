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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "core/Profile.h"

/** A named entry in the IWAD or source port list. */
struct NameEntry {
    std::string name;
    std::string file;
};

/** Remembered file dialog directories. */
struct LastDirs {
    std::string general;
    std::string wad;
    std::string src;
    std::string save;
    std::string zdl;
    std::string config;
};

/** Where the window was left, in whatever units the interface counts in. */
struct WindowGeometry {
    int width{0};
    int height{0};
    int x{0};
    int y{0};
    bool hasSize{false};
    bool hasPosition{false};
};

/** Application wide settings, the old [zdl.general] section. */
struct GeneralSettings {
    std::string alwaysAdd;
    bool autoClose{false};
    bool launchZdlImmediately{false};
    bool rememberFileList{true};
    bool showPaths{true};
    bool noUserConf{false};

    /** Which shade the interface is painted in: system, light or dark. */
    std::string theme{"system"};

    /* Bookkeeping for the "import this config to the user directory" flow. */
    bool isImported{false};
    bool doNotImportThis{false};
    std::string importedFrom;
    std::string importDate;

    WindowGeometry window;
    LastDirs lastDirs;
};

/**
 * The whole of zdl.json in memory.  This is what the interface reads from and
 * writes to; nothing above it parses a config file of any format.
 *
 * The model always holds at least one profile and activeProfileId always names
 * one of them, so activeProfile() is safe to call unconditionally.
 */
class Config {
public:
    Config();

    static constexpr int SCHEMA_VERSION = 1;

    bool load(const std::filesystem::path &path, std::string *error = nullptr);

    bool save(const std::filesystem::path &path, std::string *error = nullptr) const;

    /** Resets to a single empty profile, dropping everything else. */
    void clear();

    GeneralSettings general;
    std::vector<NameEntry> iwads;
    std::vector<NameEntry> ports;
    std::vector<Profile> profiles;
    std::string activeProfileId;

    /** The profile the launch page is currently editing. */
    Profile &activeProfile();

    [[nodiscard]] const Profile &activeProfile() const;

    [[nodiscard]] int activeProfileIndex() const;

    [[nodiscard]] int indexOfProfile(const std::string &id) const;

    /** Switches profiles.  Returns false if id names no profile. */
    bool setActiveProfile(const std::string &id);

    /** Appends a new empty profile and returns its id. */
    std::string addProfile(const std::string &name);

    /** Copies the active profile under a new name and returns the copy's id. */
    std::string duplicateActiveProfile(const std::string &name);

    /** Removes a profile; the last remaining one is emptied rather than removed. */
    void removeProfile(const std::string &id);

    /** Restores the "at least one profile, valid active id" invariant. */
    void ensureProfile();

    /** Appends " (2)", " (3)"... until the name is free. */
    [[nodiscard]] std::string uniqueProfileName(const std::string &base) const;

    [[nodiscard]] const NameEntry *findIwad(const std::string &name) const;

    [[nodiscard]] const NameEntry *findPort(const std::string &name) const;
};
