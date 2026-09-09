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

#include <span>
#include <string>
#include <vector>

/*
Cache of the releases specified in Catalog so we don't hit the origin
every time the app is loaded. Reads from and writes to a .releases
file in the data root. The cache duration is defined in KEEP.
*/
namespace Releases {
struct Answer {
    std::string portId;
    std::string version;
    std::string url;
    std::string asset;

    std::string verdict;
    std::string note;

    // When the answer was taken, in seconds since the epoch.
    long long checked{0};
    long long size{0};
};

[[nodiscard]] long long now();

// Whether an answer taken then still stands. GitHub counts its limit by the
// hour, and a release is not put out twice in half of one.
[[nodiscard]] bool fresh(long long checked);

[[nodiscard]] std::vector<Answer> read();
void write(std::span<const Answer> answers);

}
