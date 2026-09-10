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

#include <string>
#include <vector>

#include "core/util/Json.h"

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

    // Advertise on the master server. Chocolate line only.
    bool listed{false};

    friend bool operator==(const MultiplayerSettings &, const MultiplayerSettings &) = default;
};

struct ReplaySettings {
    // 0 off, 1 record, 2 play back.
    int mode{0};

    // Name inside the profile's replays folder, or a full path.
    std::string file;

    // 0 normal, 1 timed, 2 as fast as possible.
    int playback{0};

    // -complevel; -1 leaves it to the port.
    int compatibility{-1};

    bool longtics{false};
    bool soloNet{false};

    friend bool operator==(const ReplaySettings &, const ReplaySettings &) = default;
};

struct SaveSettings {
    bool enabled{false};

    // Name inside the profile's saves folder, or a full path.
    std::string file;

    friend bool operator==(const SaveSettings &, const SaveSettings &) = default;
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
    bool replayOpen{false};
    bool saveOpen{false};
    MultiplayerSettings multiplayer;
    ReplaySettings replay;
    SaveSettings save;

    std::string config;

    // Launch with the port's own config instead of the one above.
    bool sharedConfig{false};

    bool customCommand{false};

    // Placeholders: {source_port}, {game}, {addon_N}, {profile}, {cfgdir},
    // {extracfg}, {savedir}, {savefile}, {replaydir}.
    std::string command;

    bool dosFullscreen{true};
    bool captureOutput{false};

    static std::string newId();

    static Profile fromJson(yyjson_val *obj);

    [[nodiscard]] yyjson_mut_val *toJson(const Json::Builder &builder) const;

    // Everything except id, name and config.
    void clearSettings();
};
