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

#include <array>
#include <bitset>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace HexenMapInfo {

// -warp searches maps 1 to 98 only.
constexpr int LAST_WARPED = 98;

struct Listed {
    int map{0};
    int warpTrans{0};
};

// The maps -warp reaches, by map number, and the number -warp takes for each.
struct Warps {
    std::bitset<LAST_WARPED + 1> reached;
    std::array<int, LAST_WARPED + 1> warpTrans{};
};

// Each listed map once, by map number. A later entry for a map replaces the earlier one. None when the port would
// stop on the text.
[[nodiscard]] std::optional<std::vector<Listed>> parse(std::string_view text);

// From 1 to 98 for MAP01 to MAP98, any case, and 0 for any other name.
[[nodiscard]] int mapNumber(std::string_view lump);

// Such as MAP01.
[[nodiscard]] std::string lumpOf(int map);

// The maps -warp reaches with these files loaded in order, the IWAD first. Only the last MAPINFO counts, and a map
// needs its lump in one of the files. A file whose name does not end in wad is one lump named after the file. Cached
// against the size and stamp of each file.
[[nodiscard]] Warps warps(std::span<const std::string> files);

}
