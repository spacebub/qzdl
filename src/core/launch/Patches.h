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
#include <string>

#include "core/launch/Dialect.h"

namespace Patches {

// The port quits at startup on this patch, since it insists on a header and the first line is none of its signatures.
// The rule is the line reader of Chocolate Doom 3.1.0, applied to every release that insists. A file that cannot be
// opened is left to the port, and the line is read once for each file stamp.
[[nodiscard]] bool refused(const Dialect::Traits &traits, const std::string &file);

// Names the patches the launch leaves out and says why. Empty for none.
[[nodiscard]] std::string warning(const Dialect::Traits &traits, std::span<const std::string> names);

}
