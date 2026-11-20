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

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Readmes {

struct File {
    // The file name, or the entry path inside the archive.
    std::string name;
    bool inside = false;
    std::string body;
};

// Text files beside the add-on with the same stem, then those at the top of a zip, pk3 or
// folder add-on, and readmes at any depth of the zip or pk3.
[[nodiscard]] std::vector<File> of(const std::filesystem::path &addon);

// UTF-8 as it is, otherwise CP437 or Windows-1252, with line ends and tabs evened out.
[[nodiscard]] std::string readable(std::string_view bytes);

}
