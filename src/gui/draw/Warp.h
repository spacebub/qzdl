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

#include <blend2d/blend2d.h>

namespace Warp {

// A projective map of the plane in homogeneous coordinates: (x, y) goes to
// (X / W, Y / W), where [X, Y, W] is the matrix times [x, y, 1]. A perspective
// view of a flat card is one of these, which an affine is not.
struct Map {
    double m[3][3];

    [[nodiscard]] BLPoint apply(BLPoint at) const;
    [[nodiscard]] Map inverse() const;
};

// `source`, whose top left sits at `origin`, taken through `map` and laid into
// `out` for the pixels of `area`, both in the coordinates the map works in.
// Bilinear when `smooth`, else the nearest texel. Past its edge the source is
// extended, as a padded pattern is, so a transparent border stays transparent.
// `out` is kept at least as large as `area` between calls. False when nothing
// could be drawn.
bool render(const BLImage &source, BLPointI origin, const Map &map, const BLRectI &area, bool smooth,
            BLImage &out);

}
