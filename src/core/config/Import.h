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

#include <cstdint>

#include "core/config/Config.h"
#include "core/util/Ini.h"

namespace Import {

void fromLegacy(const Ini &ini, Config &config);
bool loadLegacyFile(const std::filesystem::path &path, Config &config);

Profile profileFromSection(const Ini::Section &section);
void profileToSection(const Profile &profile, Ini::Section &section);

bool loadZdlFile(const std::filesystem::path &path, Profile &profile);

enum class ZdlOpen : std::uint8_t {
    Failed,
    Added,
    Known,
};

// Adds the profile a .zdl holds, unless one was read from that file before. The id
// is the profile of the file either way. Nothing is activated.
ZdlOpen openZdlFile(Config &config, const std::filesystem::path &path, std::string &id);

bool saveZdlFile(const std::filesystem::path &path, const Profile &profile);

}
