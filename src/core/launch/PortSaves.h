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

#include <concepts>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "core/config/Config.h"
#include "core/launch/Dialect.h"

namespace PortSaves {

// Where a port saves when it is handed no save folder or ignores the one it gets. Empty unless the file name names a
// port from the download list.
[[nodiscard]] std::filesystem::path folder(const Config &config);

// DSDA-Doom and its forks organize saves unless the config they read sets dsda_organized_saves to 0.
[[nodiscard]] bool organizesSaves(const std::filesystem::path &cfg);

// The saves the port would load for the profile's game, newest first, relative to folder().
[[nodiscard]] std::vector<std::string> saves(const Config &config);

// The slot -loadgame takes to load the save of this name from the folder the port saves into.
// -1 when no slot loads it. A port not known by name is taken at the number on the end.
[[nodiscard]] int loadSlot(const Config &config, const Dialect::Traits &traits, const std::string &name);

// What the list of saves says beside a name: Auto Save, the slot as Slot 3, or nothing for a port that loads by
// name.
[[nodiscard]] std::string slotLabel(const Config &config, const Dialect::Traits &traits, const std::string &name);

// UZDoom and GZDoom read -loadgame inside the folder of the game they detect. A save from
// another game folder is reached from there. Empty when the plain name does.
[[nodiscard]] std::string loadName(const Config &config, const std::filesystem::path &save);

// Where a ZDoom port keeps saves on Windows: Saved Games, a Save folder beside the program, or the program folder.
enum class WindowsRoot : std::uint8_t {
    savedGames,
    saveBeside,
    beside,
};

// Where a ZDoom port keeps its ini on Windows. besideForUser is <name>-<user>.ini beside the program, and
// besideShared is <name>.ini there.
enum class WindowsIni : std::uint8_t {
    portable,
    myGames,
    appData,
    besideForUser,
    besideShared,
};

// Pure, so the Windows rules are tested on every platform.
[[nodiscard]] WindowsRoot windowsRoot(const Dialect::Features &features, bool underProgramFiles, bool portableIni);

// found says whether an ini is on disk. It is asked only about the inis the port reads.
template <std::predicate<WindowsIni> Found>
[[nodiscard]] WindowsIni
windowsIni(const Dialect::Features &features, const bool underProgramFiles, const Found &found) {
    const bool portableNeedsIni = features.has(Dialect::Feature::portableNeedsIni);
    const bool readsPortable = features.has(Dialect::Feature::portableIni) && !(portableNeedsIni && underProgramFiles);

    if (readsPortable && found(WindowsIni::portable)) {
        return WindowsIni::portable;
    }

    if (portableNeedsIni) {
        // Without an ini in My Games the port moves an older one there.
        for (const WindowsIni ini : {WindowsIni::myGames, WindowsIni::besideForUser, WindowsIni::appData}) {
            if (found(ini)) {
                return ini;
            }
        }

        return WindowsIni::besideShared;
    }

    const bool knownFolders = underProgramFiles && !features.has(Dialect::Feature::programFolderOnly);
    const WindowsIni written = knownFolders ? WindowsIni::appData : WindowsIni::besideForUser;

    return found(written) ? written : WindowsIni::besideShared;
}

// Where UZDoom keeps netgame saves, one folder per game below it.
[[nodiscard]] std::filesystem::path uzdoomNetgameFolder(const Dialect::Traits &traits,
                                                        const std::filesystem::path &file);

}
