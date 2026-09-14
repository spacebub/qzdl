/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
 * Copyright (C) 2018-2019  Lcferrum
 * Copyright (C) 2023-2026  spacebub
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

#include "core/config/Config.h"

namespace Storage {

// Empty for a profile that shares the port's own settings.
[[nodiscard]] std::filesystem::path configFile(const Profile &profile);
[[nodiscard]] std::filesystem::path configFile(const Config &config);

[[nodiscard]] std::filesystem::path extraConfigFile(const std::filesystem::path &config);

[[nodiscard]] std::filesystem::path saveDirectory(const Config &config);

// saveDirectory, or empty when the port has no switch for one.
[[nodiscard]] std::filesystem::path saveFolder(const Config &config);

// Newest first. Names, not paths.
[[nodiscard]] std::vector<std::string> saves(const Config &config);

[[nodiscard]] std::filesystem::path saveFile(const Config &config);

// The number on the end of the name; -1 when there is none.
[[nodiscard]] int saveSlot(const std::string &name);

[[nodiscard]] std::string saveTrouble(const Config &config);

// Holds the profile's config, whether or not the port shares one; empty when there is none.
[[nodiscard]] std::filesystem::path profileDirectory(const Profile &profile);

// Whether the folder is ZDL's to move or delete: a plain name of its own, shared with
// no other profile.
[[nodiscard]] bool ownsDirectory(const Config &config, const Profile &profile);

// Moves the folder and the configs named after it. profile.config is taken up only
// once the move is through; a failure leaves everything where it was.
bool renameDirectory(Profile &profile, const std::string &file, std::string *error = nullptr);

// The folder and everything in it: the config, the saves and the replays.
bool discardDirectory(const Profile &profile, std::string *error = nullptr);

// levelstat.txt, screenshots and the like are written where the port runs, so a profile runs
// in its own directory rather than the port's.
[[nodiscard]] std::filesystem::path runDirectory(const Config &config,
                                                 const std::filesystem::path &portDirectory);

// The config the port writes for this profile; a DOS port gets a name DOS can spell.
[[nodiscard]] std::filesystem::path portConfigFile(const Config &config, const Profile &profile);

// True with nothing done when either profile has no config, or they share one.
bool copyPortConfig(const Config &config, const Profile &from, const Profile &to,
                    std::string *error = nullptr);

// Beside the profile's config, whether or not the port shares one.
[[nodiscard]] std::filesystem::path replayDirectory(const Profile &profile);
[[nodiscard]] std::filesystem::path replayDirectory(const Config &config);

// Newest first. Names, not paths.
[[nodiscard]] std::vector<std::string> replays(const Config &config);

[[nodiscard]] std::filesystem::path replayFile(const Config &config);

[[nodiscard]] std::string replayTrouble(const Config &config);

}
