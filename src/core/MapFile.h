/*
 * This file is part of qZDL
 * Copyright (C) 2007-2012  Cody Harris
 * Copyright (C) 2018-2019  Lcferrum
 * Copyright (C) 2023-2026  spacebub
 *
 * qZDL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

/**
 * Anything maps can be read out of: a WAD, a zipped PK3, or a loose directory
 * laid out like one.  What the file actually is decides which of those it is
 * read as, not what it is called.
 */
class MapFile {
public:
    MapFile() = default;

    /** Opens whatever kind of map file this is, or nothing when it is none. */
    static std::unique_ptr<MapFile> open(const std::filesystem::path &file);

    /** The name an IWADINFO lump gives this IWAD, or empty when it has none. */
    virtual std::string iwadinfoName() = 0;

    virtual std::vector<std::string> mapNames() = 0;

    /** True when maps here are named MAPxx rather than ExMy. */
    virtual bool isMapXX() = 0;

    virtual ~MapFile() = default;

    MapFile(const MapFile &) = delete;

    MapFile &operator=(const MapFile &) = delete;

    MapFile(MapFile &&) = delete;

    MapFile &operator=(MapFile &&) = delete;

protected:
    /**
     * Pulls the Name = "..." out of an IWADINFO lump, which is the same shape
     * whether it came out of a WAD, a zip entry or a file on disk.
     */
    static std::string nameFromIwadinfo(std::string_view text);
};
