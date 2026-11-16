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

#include "ttk/system/Json.h"

namespace Versions {

// The format version the reader and the writer know.
inline constexpr int CURRENT = 9;

// What a config file says about its format: the version and the release that wrote it.
struct Stamp {
    int version{CURRENT};
    std::string release;
};

// The format version of a config file. Releases up to 4.5.0 wrote 1, so the release decides for a file that says 1
// or gives no usable number. A file with neither number nor release comes from no ZDL4 and counts as CURRENT.
[[nodiscard]] int of(const yyjson_val *root);

// The document brought up to CURRENT, one step at a time. `from` is what of() returned for it. Not valid when
// memory runs out.
[[nodiscard]] ttk::Json::Doc upgraded(const ttk::Json::Doc &doc, int from);

}
