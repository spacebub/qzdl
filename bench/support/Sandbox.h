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

#include <filesystem>

namespace bench {

// A scratch tree the config, data and cache paths are pointed at, so no
// benchmark reads or writes the user's own ZDL files.
namespace Sandbox {

// Before anything reads an environment variable. Paths caches what it finds.
void enter();

void leave();

const std::filesystem::path &root();

// Empty and recreated. A benchmark that writes gets a clean one.
std::filesystem::path scratch(const char *name);

}

}
