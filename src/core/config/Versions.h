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

#include "ttk/system/Json.h"

namespace Versions {

// The shape the reader and the writer know.
inline constexpr int CURRENT = 9;

// The shape a config file is in. A file that says 1 was written before the number meant anything, so the release
// that wrote it tells.
[[nodiscard]] int of(const yyjson_val *root);

// The document brought up to CURRENT, one step at a time. `from` is what of() returned for it. Not valid when
// memory runs out.
[[nodiscard]] ttk::Json::Doc upgraded(const ttk::Json::Doc &doc, int from);

}
