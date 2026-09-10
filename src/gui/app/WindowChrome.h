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

#include <cstdint>

namespace slint {
class Window;
}

// What a frameless window still needs from Windows: rounding, outline, shadow, snapping and maximize bounds.
namespace WindowChrome {

// Must run inside the event loop; the native window only exists once it starts.
void apply(slint::Window &window);

void outline(uint8_t red, uint8_t green, uint8_t blue);

// Hands the drag to Windows so it snaps. False when there is no window yet.
bool beginMove();

}
