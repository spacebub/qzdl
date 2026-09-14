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

#include <algorithm>
#include <cstddef>
#include <vector>

namespace Cache {

// Throws away the quarter of `held` that has gone longest unasked for. Entries carry
// a `used` stamp taken from a counter the caller bumps on every lookup.
//
// Clearing the lot instead is what makes a page holding more than the cap thrash:
// everything on it is rebuilt every frame, for as long as it is up.
template <typename Map, typename Dropped = decltype([](const auto &) {})>
void evictOldest(Map &held, const size_t keep, Dropped dropped = {}) {
    if (held.size() < keep) {
        return;
    }

    std::vector<size_t> stamps;

    stamps.reserve(held.size());

    for (const auto &entry : held) {
        stamps.push_back(entry.second.used);
    }

    const size_t quarter = stamps.size() / 4;

    std::ranges::nth_element(stamps, stamps.begin() + static_cast<ptrdiff_t>(quarter));

    const size_t oldest = stamps[quarter];

    std::erase_if(held, [oldest, &dropped](const auto &entry) {
        if (entry.second.used > oldest) {
            return false;
        }

        dropped(entry.second);

        return true;
    });
}

}
