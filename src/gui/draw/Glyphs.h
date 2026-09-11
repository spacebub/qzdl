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

// Every icon in the application, as path data. None of them is a bitmap, so they
// stay sharp at every size and take whatever tone they are given.
namespace Glyphs {

// The whole element is a `12 * weight` square; the drawing inside it is centred
// and sized from its own viewbox.
constexpr float element = 12.0F;

// Draws `name` with its top-left at `origin`. Unknown names draw nothing. `turn`
// is in degrees about the square's centre.
void draw(BLContext &context, const char *name, BLPoint origin, float weight, BLRgba32 tone,
          float turn = 0.0F);

// The side of the square `draw` covers.
float span(float weight);

// Every glyph in the table, drawn onto one sheet and written to `path`. This is
// the whole icon set of the application, rendered from the same strings it uses.
bool sheet(const char *path);

}
