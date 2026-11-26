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

#include <functional>
#include <string>

#include <blend2d/blend2d.h>

#include "ttk/toolkit/Widget.h"

#include "gui/components/Reach.h"
#include "gui/model/GroupsBridge.h"

namespace components {

// Rename, Move up, Move down and Delete group for the group `id` of `list`, dropped under `button`.
// `closed` runs once the menu is gone.
void showGroupMenu(Reach &reach,
                   const ttk::Widget &owner,
                   GroupsBridge::List list,
                   std::uint16_t id,
                   const std::string &name,
                   const BLRect &button,
                   std::function<void()> closed);

}
