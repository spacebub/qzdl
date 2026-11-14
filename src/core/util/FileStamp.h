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

// What a file or folder looked like when it was read, to tell whether it has changed since.
struct FileStamp {
    bool there{false};
    std::uintmax_t size{0};
    std::int64_t seconds{0};
    std::int64_t nanoseconds{0};

    bool operator==(const FileStamp &) const = default;

    // A folder has no size.
    static FileStamp of(const std::filesystem::path &path);

    // Skips building a path where the system takes the name as it is.
    static FileStamp of(const std::string &path);
};
