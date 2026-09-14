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
#include <vector>

// The file patterns the pickers ask for, one list per kind of file ZDL opens.
namespace Filters {

const std::vector<std::string> &wad();
const std::vector<std::string> &port();
const std::vector<std::string> &zdl();
const std::vector<std::string> &config();
const std::vector<std::string> &save();
const std::vector<std::string> &replay();

}
