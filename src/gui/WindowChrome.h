/*
 * This file is part of qZDL
 * Copyright (C) 2026  spacebub
 *
 * qZDL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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

// What a window wearing its own decoration still has to ask Windows for: the
// sliver of frame the compositor rounds, outlines and shadows, the drag that
// docks against an edge, and how much of the screen a maximized one gets.
namespace WindowChrome {

// Taken over from inside the event loop, since showing the window does not make
// one: the backend makes it when the loop starts.
void apply(slint::Window &window);

// The one part in the interface's colours, so redone with the shade.
void outline(uint8_t red, uint8_t green, uint8_t blue);

// Hands a drag over to Windows, which is what makes it snap. False where there
// is nobody to hand it to yet.
bool beginMove();

}
