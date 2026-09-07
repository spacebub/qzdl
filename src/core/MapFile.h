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
#include <string>
#include <vector>

class MapFile {
public:
    MapFile() = default;

    static std::unique_ptr<MapFile> open(const std::filesystem::path &file);

    virtual std::string iwadinfoName() = 0;

    // The named lump, entry or file, byte for byte, or nothing when there
    // is none. Names match the way Doom matches them: case insensitively.
    virtual std::string lump(std::string_view name) = 0;

    // Every name the file has something under, in capitals. Duplicates and
    // all, since it says what is in the file rather than what can be read.
    virtual std::vector<std::string> lumpNames() = 0;

    // Whether the file is a game in its own right rather than something
    // loaded on top of one. Only a game is the authority on its own colours.
    virtual bool isGame() = 0;

    virtual std::vector<std::string> mapNames() = 0;

    virtual bool isMapXX() = 0;

    virtual ~MapFile() = default;

    MapFile(const MapFile &) = delete;

    MapFile &operator=(const MapFile &) = delete;

    MapFile(MapFile &&) = delete;

    MapFile &operator=(MapFile &&) = delete;

protected:
    // Pulls the Name = "..." out of an IWADINFO lump, which is the same shape
    // whether it came out of a WAD, a zip entry or a file on disk.
    static std::string nameFromIwadinfo(std::string_view text);
};
