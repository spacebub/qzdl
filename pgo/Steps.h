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

#include <span>

#include "Drive.h"
#include "World.h"

namespace pgo {

// One thing a player does in a sitting, in the order a sitting goes.
struct Step {
    const char *name;
    void (*run)(Drive &drive, const World &world);
};

std::span<const Step> steps();

// What runs outside the window: the config and the game files, read and written
// as a start, a save and a launch read and write them.
void core(const World &world, Tally &tally);

}
