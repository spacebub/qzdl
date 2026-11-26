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

namespace Band {

// The header of a group, raised off the sunken trough it sits in.
inline void paint(const ttk::Painter &painter, const BLRect &box) {
    painter.round(box, ttk::Theme::radiusSmall - 2.0, ttk::Theme::palette().raised);
}

}
