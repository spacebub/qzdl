/*
 * This file is part of qZDL
 * Copyright (C) 2007-2012  Cody Harris
 * Copyright (C) 2018-2019  Lcferrum
 * Copyright (C) 2023-2026  spacebub
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
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

class MapFile {
public:
    struct Maps {
        bool opened{false};
        bool mapxx{false};
        std::vector<std::string> names;
    };

    MapFile() = default;

    static std::unique_ptr<MapFile> open(const std::filesystem::path &file);

    // Cached against the file's size and stamp.
    static const Maps &maps(const std::string &file);

    virtual std::string iwadinfoName() = 0;

    // Case insensitive.
    virtual std::string lump(std::string_view name) = 0;

    // The earliest name in the list wins; entries that cannot be pictures are skipped.
    virtual std::string picture(std::span<const std::string_view> names) = 0;

    // Uppercase, duplicates included.
    virtual std::vector<std::string> lumpNames() = 0;

    virtual bool isGame() = 0;

    virtual std::vector<std::string> mapNames() = 0;

    virtual bool isMapXX() = 0;

    virtual ~MapFile() = default;

    MapFile(const MapFile &) = delete;

    MapFile &operator=(const MapFile &) = delete;

    MapFile(MapFile &&) = delete;

    MapFile &operator=(MapFile &&) = delete;

protected:
    // The Name = "..." of an IWADINFO.
    static std::string nameFromIwadinfo(std::string_view text);

    // Index in names, or names.size() when absent.
    static size_t rankOf(std::span<const std::string_view> names, std::string_view stem);

    // A picture extension, none at all, or a graphics directory.
    static bool drawable(std::string_view name, std::string_view directory);
};
