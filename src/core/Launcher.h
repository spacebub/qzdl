/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
 * Copyright (C) 2018-2019  Lcferrum
 * Copyright (C) 2023-2026  spacebub
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

#include "core/Config.h"
#include "core/Process.h"
#include "core/Profile.h"

namespace Launcher {

[[nodiscard]] std::filesystem::path executable(const Config &config);

[[nodiscard]] std::vector<std::string> arguments(const Config &config);

[[nodiscard]] std::filesystem::path getConfigPath(const Profile &profile);
[[nodiscard]] std::filesystem::path getConfigPath(const Config &config);
[[nodiscard]] std::filesystem::path getSavePath(const Config &config);

[[nodiscard]] std::string commandLine(const Config &config);

/*
A profile that writes its own command line, as the tokens it comes out as:
the program first and its arguments after it, with {source_port}, {game},
{addon_1} upwards, {profile}, {cfgdir} and {savedir} filled in. Empty is one
that cannot be run.
*/
[[nodiscard]] std::vector<std::string> customCommand(const Config &config,
                                                     std::string *error = nullptr);

// What is wrong with the command the profile writes itself, or nothing at all.
[[nodiscard]] std::string commandTrouble(const Config &config);

/*
What this profile would have been launched with, written the way a custom
command is written: the port, the game and the add-ons put back as the words
that stand for them. It is what taking the command over starts from.
*/
[[nodiscard]] std::string commandTemplate(const Config &config);

// A port marked as a DOS program is not started itself: DOSBox is, with the
// directories the launch names mounted as drives and the port run off C:.
[[nodiscard]] bool isDosPort(const Config &config);

/*
The DOSBox a config launches with: the one it names, or failing that whatever
this machine already has. Empty is a machine with none and a config that has
not been pointed at one.
*/
[[nodiscard]] std::filesystem::path dosbox(const Config &config);

// What was found on this machine, which is what an unset config falls back on.
[[nodiscard]] std::filesystem::path systemDosbox();

// The id is the hold on the game that comes back, for asking later whether it
// is still up; the stream is its output, and is only piped when asked for.
bool launch(const Config &config,
            Process::Id *id = nullptr,
            Process::Stream *output = nullptr,
            std::string *error = nullptr);

[[nodiscard]] std::vector<std::string> maps(const Config &config);

}
