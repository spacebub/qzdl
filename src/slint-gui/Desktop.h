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

#include <filesystem>
#include <string>
#include <vector>

// What the interface needs of the system and Slint has no answer for.
namespace Desktop {

// Hands a path or url to whatever the desktop opens it with; `why` takes the failure.
bool open(const std::string &target, std::string *why = nullptr);

// Windows drives, where browsing up far enough lands. Empty elsewhere.
[[nodiscard]] std::vector<std::string> drives();

// Whatever this machine has that a path reads well in.
[[nodiscard]] std::string monospaceFamily();

}
