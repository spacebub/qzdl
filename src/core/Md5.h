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

/*
MD5, for the one thing ZDL uses a hash for: looking an IWAD up in a table of
known releases. Nothing here is trusted against tampering, and MD5 is what
every published table of IWAD digests is written in, so it is what identifies
one.
*/
namespace Md5 {

/** The digest of a whole file as lower case hex, or empty if it cannot be read. */
[[nodiscard]] std::string ofFile(const std::filesystem::path &file);

}
