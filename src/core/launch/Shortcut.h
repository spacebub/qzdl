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
#include <string_view>

// A desktop icon on Windows. Elsewhere an XDG desktop entry in the applications menu.
namespace Shortcut {

inline constexpr const char *CREATE = "Create desktop shortcut";
inline constexpr const char *REMOVE = "Remove desktop shortcut";

// The running ZDL4. argv[0] may be relative to a folder the desktop never starts it from.
[[nodiscard]] std::filesystem::path program();

// A config, when given, is read instead of the one a plain start finds. A shortcut that already launches
// the same is written over. Windows draws the icon from the program and ignores `iconPng`. Empty on
// failure, with error set.
std::filesystem::path make(const std::string &stem,
                           const std::filesystem::path &zdl,
                           const std::filesystem::path &config,
                           std::string_view iconPng,
                           std::string *error);

}
