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

#include <string>

#include <blend2d/blend2d.h>

// The drawing the application needs that Blend2D does not do for it.
namespace Paint {

// A drop shadow, as a sprite: Blend2D has no blur of any kind, so a rounded
// rectangle is blurred by hand and kept, cached by its shape. `blur` is read the
// way CSS reads it, as twice the Gaussian sigma.
const BLImage &shadow(int width, int height, double radius, double blur, BLRgba32 tint);

// Where the sprite's top-left goes if the box it belongs to is at (x, y).
double bleed(double blur);

// A vertical gradient across `box`, top to bottom.
BLGradient down(const BLRect &box);

// An image, or an empty one. Blend2D decodes PNG itself.
BLImage load(const std::string &path);

// `source` drawn to fill `box` and cropped to it, which is CSS's `cover`. A radius
// rounds the corners off what is drawn.
void cover(BLContext &context, const BLRect &box, const BLImage &source, double radius = 0.0);

}
