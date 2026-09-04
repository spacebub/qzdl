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

#include <string>
#include <vector>

#include "core/Json.h"

struct FileEntry {
    std::string file;
    bool enabled{true};
};

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

struct Profile {
    std::string id;
    std::string name;
    std::string iwad;
    std::string port;
    std::vector<FileEntry> files;
    int skill{0};
    int monsters{0};
    std::string warp;
    std::string extra;
    bool dialogOpen{false};
    MultiplayerSettings multiplayer;

    std::string config;
    // Launches with the port's own config instead of the one above.
    bool sharedConfig{false};

    /** Whether a launch takes the game's output, which needs a terminal for it. */
    bool captureOutput{true};

    static std::string newId();

    static Profile fromJson(yyjson_val *obj);

    [[nodiscard]] yyjson_mut_val *toJson(const Json::Builder &builder) const;

    // Everything except id and name, used when switching or clearing.
    void clearSettings();
};
