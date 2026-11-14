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

#include "ttk/draw/Theme.h"
#include "ttk/toolkit/Painter.h"
#include "ttk/toolkit/Widget.h"

namespace DropMark {

inline constexpr double WIDTH = 2.0;

inline void paint(const ttk::Painter &painter, const BLRect &box, const double radius) {
    const BLRect edge{box.x + 1.0, box.y + 1.0, box.w - 2.0, box.h - 2.0};

    painter.outline(edge, radius, WIDTH, ttk::Theme::palette().accent);
}

// Only the four edges, as the inside does not change.
inline void invalidate(const ttk::Widget &who, const BLRect &box, const double radius) {
    const double band = WIDTH + radius + 2.0;

    who.invalidate(BLRect{box.x, box.y, box.w, band});
    who.invalidate(BLRect{box.x, box.y + box.h - band, box.w, band});
    who.invalidate(BLRect{box.x, box.y, band, box.h});
    who.invalidate(BLRect{box.x + box.w - band, box.y, band, box.h});
}

}
