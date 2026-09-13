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

#include "core/config/Config.h"

namespace bench {

// Deterministic shapes, so a number means the same thing on the next commit.
namespace Fixtures {

Config config(int profiles, int filesPerProfile);

// Paths point at the corpus, so the readers open real files.
Config grounded(int profiles, int filesPerProfile);

void install(Config what);

std::string words(int count, unsigned seed = 1);

std::vector<std::string> lines(int count);

std::string paragraph(int sentences);

}

}
