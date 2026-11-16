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

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "core/config/Config.h"
#include "core/launch/Dialect.h"

namespace PortSaves {

// Where a port keeps saves on its own settings, for a profile without a folder of its own. Empty
// unless the file name names a port from the download list.
[[nodiscard]] std::filesystem::path folder(const Config &config);

// The saves the port would load for the profile's game, newest first, relative to folder().
[[nodiscard]] std::vector<std::string> saves(const Config &config);

// The slot -loadgame takes to load the save of this name from the folder the port saves into.
// -1 when no slot loads it. A port not known by name is taken at the number on the end.
[[nodiscard]] int loadSlot(const Config &config, const Dialect::Port &port, const std::string &name);

// UZDoom and GZDoom read -loadgame inside the folder of the game they detect. A save from
// another game folder is reached from there. Empty when the plain name does.
[[nodiscard]] std::string loadName(const Config &config, const std::filesystem::path &save);

// Where a ZDoom port keeps saves on Windows: Saved Games, a Save folder beside the program, or the program folder.
enum class WindowsRoot : std::uint8_t {
    savedGames,
    saveBeside,
    beside,
};

// Where a ZDoom port keeps its ini on Windows. besideForUser is <name>-<user>.ini beside the program.
enum class WindowsIni : std::uint8_t {
    portable,
    myGames,
    appData,
    besideForUser,
};

// Pure, so the Windows rules are tested on every platform.
[[nodiscard]] WindowsRoot windowsRoot(const Dialect::Features &features, bool underProgramFiles, bool portableIni);
[[nodiscard]] WindowsIni windowsIni(const Dialect::Features &features, bool underProgramFiles, bool portableIni);

// Where UZDoom keeps netgame saves, one folder per game below it.
[[nodiscard]] std::filesystem::path uzdoomNetgameFolder(const Dialect::Port &port,
                                                        const std::filesystem::path &program);

}
