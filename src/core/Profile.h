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

#include <string>
#include <vector>

#include "core/Json.h"

/**
 * One entry of the external file list.  Entries can be disabled, which keeps
 * them in the list (struck through in the UI) but off the command line; the INI
 * format encoded that as a "d" suffix on the fileN key.
 */
struct FileEntry {
    std::string file;
    bool enabled{true};
};

/**
 * Multiplayer half of a launch profile.  The text fields are kept as strings
 * rather than numbers because an empty string is what "not set" means here, and
 * the source port command line distinguishes an unset limit from a zero one.
 */
struct MultiplayerSettings {
    int gameType{0};
    int players{0};
    int extratic{0};
    int netmode{-1};
    int dup{0};
    std::string host;
    std::string port;
    std::string fragLimit;
    std::string timeLimit;
    std::string dmflags;
    std::string dmflags2;
    std::string savegame;
};

/**
 * A single named launch configuration.  This replaces the old [zdl.save]
 * section: where there used to be exactly one, a config now holds a list.
 *
 * Every field is just whatever the user picked while this profile was the
 * active one; nothing here ties a profile to a particular game or port.
 *
 * iwad and port refer to entries in the config's IWAD and source port lists
 * *by name*, which is both how [zdl.save] always worked and what .zdl files
 * exchanged with other Doom tools expect.  Either may be empty when unset.
 */
struct Profile {
    std::string id;
    std::string name;
    std::string iwad;
    std::string port;
    std::vector<FileEntry> files;
    /* 0 means "not set" for both of these, matching the combo box index where
     * entry 0 is the empty default. */
    int skill{0};
    int monsters{0};
    std::string warp;
    std::string extra;
    bool dialogOpen{false};
    MultiplayerSettings multiplayer;

    /** Generates a fresh unique profile id. */
    static std::string newId();

    static Profile fromJson(yyjson_val *obj);

    [[nodiscard]] yyjson_mut_val *toJson(const Json::Builder &builder) const;

    /** Everything except id and name, used when switching or clearing. */
    void clearSettings();
};
