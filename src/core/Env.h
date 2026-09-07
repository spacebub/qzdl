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

// The one place the environment is read. MSVC deprecates getenv in favour of a
// call of its own, so the difference is kept here rather than at every use.
namespace Env {

// A variable's value, empty where it is unset. Nothing here tells that apart
// from a variable set to nothing, because nothing needs to.
[[nodiscard]] std::string get(const char *name);

}
