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
#include <string>

namespace tests {

struct FlatpakApp {
    std::filesystem::path exported;
    std::filesystem::path commit;
};

// An app deployed under root as flatpak lays it out, with the export linked into the commit through current and
// active. A second deploy in the same root moves the links.
FlatpakApp
deployFlatpak(const std::filesystem::path &root, const std::string &app, const std::string &branch = "stable");

}
