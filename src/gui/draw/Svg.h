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

// SVG path data into a BLPath.
//
// Every icon in the application is a path string, so none of them is redrawn or
// rasterised by hand. Blend2D parses no such thing of its own, though everything
// under it -- arcs, smooth continuations, relative coordinates -- it does have.
namespace Svg {

// Fills `out`, which is cleared first. False on the first token that makes no
// sense, with whatever parsed cleanly left in place.
bool parse(const char *commands, BLPath &out);

// Scaled from a viewbox of `box` units square into a `size` square.
BLPath glyph(const char *commands, float box, float size);

}
