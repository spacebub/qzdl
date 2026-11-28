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

#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "core/config/Profile.h"

namespace FileBrowser {

struct Entry {
    std::string path;
    std::string name;

    // How many folders down from the folder listed.
    std::size_t depth = 0;
    bool folder = false;
};

// Stops a parent folder such as a home or a drive root from holding up the UI.
inline constexpr std::size_t SCAN_CAP = 20000;

// The tree below `folder` in display order: each folder is followed by what it holds, folders before files, each in
// natural order by name. Only files `wanted` takes are listed, and only folders that hold one at some depth. What
// cannot be read is left out. The walk stops after `cap` entries, files and folders alike.
[[nodiscard]] std::vector<Entry> scan(const std::filesystem::path &folder,
                                      const std::function<bool(std::string_view name)> &wanted,
                                      std::size_t cap = SCAN_CAP);

// `rows` holds the fileKey of each row. Makes `picks` the files of the list among the rows. Kept picks stay in place
// and the other rows go. New picks follow the last pick in load order and join its group, so a swap lands where the
// old pick was. With no pick they go to the top ungrouped. True on a change.
bool pick(std::vector<FileEntry> &files, const std::vector<std::string> &rows, const std::vector<std::string> &picks);

}
