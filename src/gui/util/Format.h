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

// Path and text shaping the interface asks for.
namespace Format {

// The interface expects forward slashes.
[[nodiscard]] inline std::string fromPath(const std::filesystem::path &path) {
    return path.generic_string();
}

// Home written as ~.
[[nodiscard]] std::string prettyPath(const std::string &path);

// Drops whole leading directories to fit; zero is no limit.
[[nodiscard]] std::string fitPath(const std::string &path, int room);

[[nodiscard]] std::string directoryOf(const std::string &path);

[[nodiscard]] std::string fileName(const std::string &path);

[[nodiscard]] bool isFile(const std::string &path);
[[nodiscard]] bool isDirectory(const std::string &path);

// Case and slash insensitive on Windows.
[[nodiscard]] bool sameFile(const std::string &left, const std::string &right);

[[nodiscard]] std::string upper(const std::string &value);

// 12.4 MB and the like.
[[nodiscard]] std::string bytes(unsigned long long size);

}
