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

#include <vector>

#include <blend2d/blend2d.h>

// Which rectangles of a frame changed. The window and the benchmark canvas both
// keep one, so a change to how regions are merged cannot leave the two measuring
// different things.
class Damage {
public:
    // More than this many separate rectangles and it is cheaper to present one that
    // covers them all than to hand the desktop a long list.
    static constexpr size_t CROWDED = 12;

    void resize(int width, int height);

    // Clamped to the frame, dropped when something held already covers it, and the
    // lot folded into one once there are too many.
    void add(const BLRect &region);

    void all();

    void clear() { _regions.clear(); }

    [[nodiscard]] bool empty() const { return _regions.empty(); }

    [[nodiscard]] const std::vector<BLRectI> &regions() const { return _regions; }

    // `region` clipped to the frame. Empty when none of it is inside.
    [[nodiscard]] BLRectI clampTo(const BLRect &region) const;

private:
    std::vector<BLRectI> _regions;

    int _width = 0;
    int _height = 0;
};
