/*
 * This file is part of qZDL
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <filesystem>
#include <string>

/*
What to call a file the user has just added, so a list of paths reads as a
list of games and ports rather than as a list of paths.
*/
namespace FileInfo {

/** The name of the game in an IWAD, falling back to the file's own name. */
[[nodiscard]] std::string describeIwad(const std::filesystem::path &file);

/** The proper name of a source port executable, falling back to its stem. */
[[nodiscard]] std::string describePort(const std::filesystem::path &file);

}
