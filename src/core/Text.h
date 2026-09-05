/*
 * This file is part of qZDL
 * Copyright (C) 2026  spacebub
 *
 * qZDL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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
#include <string_view>
#include <vector>

namespace Text {

[[nodiscard]] std::string lower(std::string_view value);

[[nodiscard]] std::string upper(std::string_view value);

[[nodiscard]] std::string trim(std::string_view value);

[[nodiscard]] bool iequals(std::string_view left, std::string_view right);

[[nodiscard]] bool iendsWith(std::string_view value, std::string_view suffix);

[[nodiscard]] int toInt(std::string_view value, int def = 0);

[[nodiscard]] bool isInt(std::string_view value);

[[nodiscard]] std::vector<std::string> split(std::string_view value, char separator);

[[nodiscard]] std::string join(const std::vector<std::string> &parts, std::string_view separator);

/*
Less comparison for natural sorting, so MAP2 comes before MAP10.

Based on "The Alphanum Algorithm" by David Koelle, released under the MIT
License. http://www.davekoelle.com/alphanum.html
*/
[[nodiscard]] bool naturalLess(std::string_view left, std::string_view right);

[[nodiscard]] std::vector<std::string> parseArguments(std::string_view line);

[[nodiscard]] std::string quoteArgument(std::string_view argument);

}
