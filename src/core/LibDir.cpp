/*
 * This file is part of qZDL
 * Copyright (C) 2019  Lcferrum
 * Copyright (C) 2023-2026  spacebub
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

#include <fstream>
#include <sstream>
#include <utility>

#include "core/LibDir.h"
#include "core/Text.h"

LibDir::LibDir(std::filesystem::path file) : _file(std::move(file)) {
}

std::filesystem::path LibDir::mapsDirectory() const {
    std::error_code code;

    for (const auto &entry : std::filesystem::directory_iterator(_file, code)) {
        if (entry.is_directory(code) && Text::iequals(entry.path().filename().string(), "maps")) {
            return entry.path();
        }
    }

    return {};
}

std::vector<std::string> LibDir::mapNames() {
    std::vector<std::string> names;
    std::error_code code;

    for (const auto &entry : std::filesystem::directory_iterator(_file, code)) {
        if (!entry.is_regular_file(code)) {
            continue;
        }

        if (const std::unique_ptr<MapFile> map = MapFile::open(entry.path())) {
            std::vector<std::string> found = map->mapNames();

            names.insert(names.end(), found.begin(), found.end());
        }
    }

    const std::filesystem::path maps = mapsDirectory();

    if (maps.empty()) {
        return names;
    }

    // Inside "maps" the file name is the map name, cut to the eight characters
    // a lump name is allowed.
    for (const auto &entry : std::filesystem::directory_iterator(maps, code)) {
        if (!entry.is_regular_file(code)) {
            continue;
        }

        names.push_back(Text::upper(entry.path().stem().string().substr(0, 8)));
    }

    return names;
}

std::string LibDir::iwadinfoName() {
    std::error_code code;

    for (const auto &entry : std::filesystem::directory_iterator(_file, code)) {
        if (!entry.is_regular_file(code) || !Text::iequals(entry.path().stem().string(), "iwadinfo")) {
            continue;
        }

        std::ifstream const file(entry.path(), std::ios::binary);

        if (!file) {
            continue;
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();

        return nameFromIwadinfo(buffer.str());
    }

    return {};
}

namespace {

// The file in this one directory whose stem matches, read whole.
std::string fileNamed(const std::filesystem::path &directory, const std::string_view name) {
    std::error_code code;

    for (const auto &entry : std::filesystem::directory_iterator(directory, code)) {
        if (!entry.is_regular_file(code) || !Text::iequals(entry.path().stem().string(), name)) {
            continue;
        }

        std::ifstream const stream(entry.path(), std::ios::binary);

        if (!stream) {
            continue;
        }

        std::ostringstream buffer;
        buffer << stream.rdbuf();

        return buffer.str();
    }

    return {};
}

}

std::string LibDir::lump(const std::string_view name) {
    if (std::string bytes = fileNamed(_file, name); !bytes.empty()) {
        return bytes;
    }

    std::error_code code;

    // One level down as well, which is as deep as a directory laid out like a
    // PK3 puts its graphics.
    for (const auto &entry : std::filesystem::directory_iterator(_file, code)) {
        if (!entry.is_directory(code)) {
            continue;
        }

        if (std::string bytes = fileNamed(entry.path(), name); !bytes.empty()) {
            return bytes;
        }
    }

    return {};
}

std::vector<std::string> LibDir::lumpNames() {
    std::vector<std::string> names;
    std::error_code code;

    // One level down as well, which is as deep as lump() reads.
    for (const auto &entry : std::filesystem::directory_iterator(_file, code)) {
        if (entry.is_regular_file(code)) {
            names.push_back(Text::upper(entry.path().stem().string()));
            continue;
        }

        if (!entry.is_directory(code)) {
            continue;
        }

        for (const auto &nested : std::filesystem::directory_iterator(entry.path(), code)) {
            if (nested.is_regular_file(code)) {
                names.push_back(Text::upper(nested.path().stem().string()));
            }
        }
    }

    return names;
}

bool LibDir::isGame() {
    return !lump("IWADINFO").empty();
}

bool LibDir::isMapXX() {
    const std::filesystem::path maps = mapsDirectory();

    if (maps.empty()) {
        return false;
    }

    std::error_code code;

    for (const auto &entry : std::filesystem::directory_iterator(maps, code)) {
        const std::string name = entry.path().filename().string();

        if (Text::iequals(name, "map01.wad") || Text::iequals(name, "map01.map")) {
            return true;
        }
    }

    return false;
}
