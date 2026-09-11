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

struct SDL_Window;

// What a frameless window still needs from the desktop: rounding, outline, shadow
// and maximize bounds. Dragging and resizing go through SDL's hit test instead,
// which the platform turns into its own move and resize.
namespace Chrome {

// Safe to call right after the window is created; the native handle already exists.
void apply(SDL_Window *window);

void outline(std::uint8_t red, std::uint8_t green, std::uint8_t blue);

// Called with true when the desktop opens a modal move/resize loop of its own and
// false when it closes it. Never called where there is no such thing.
void whileResizing(void (*told)(bool));

}
