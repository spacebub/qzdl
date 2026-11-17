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
#include <filesystem>
#include <string>

namespace FileVersion {

// The FileVersion string in the version resource of a Windows program, such as g4.14.2-0-g1234abc. Empty off
// Windows and for a file without one.
[[nodiscard]] std::string of(const std::filesystem::path &program);

// Where the string lies for a translation of the resource, such as \StringFileInfo\040904b0\FileVersion.
[[nodiscard]] std::string key(std::uint16_t language, std::uint16_t codepage);

}
