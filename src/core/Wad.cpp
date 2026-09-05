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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <fstream>
#include <utility>

#include "core/Text.h"
#include "core/Wad.h"

namespace {

// A WAD that claims more lumps than this is a WAD that has been truncated or
// corrupted, and reading it would be an allocation of hundreds of megabytes.
constexpr std::int32_t LUMP_LIMIT = 1 << 20;

}

Wad::Wad(std::filesystem::path file) : _file(std::move(file)) {
}

std::string_view Wad::lumpName(const Lump &lump) {
    const size_t length = std::string_view(lump.name, sizeof(lump.name)).find('\0');

    return {lump.name, length == std::string_view::npos ? sizeof(lump.name) : length};
}

std::vector<Wad::Lump> Wad::readDirectory(std::ifstream &stream) {
    Header header{};

    if (!stream.read(reinterpret_cast<char *>(&header), sizeof(header))) {
        return {};
    }

    if (header.lumps <= 0 || header.lumps > LUMP_LIMIT || header.directory < 0) {
        return {};
    }

    std::vector<Lump> lumps(static_cast<size_t>(header.lumps));

    stream.seekg(header.directory);

    if (!stream.read(reinterpret_cast<char *>(lumps.data()),
                     static_cast<std::streamsize>(lumps.size() * sizeof(Lump)))) {
        return {};
    }

    return lumps;
}

std::vector<std::string> Wad::mapNames() {
    std::vector<std::string> names;
    std::ifstream stream(_file, std::ios::binary);

    if (!stream) {
        return names;
    }

    /*
    Generally the WAD structure follows a simple layout, and we can assume that
    it will hold for most WADs. In most cases map lumps follow the pattern:
    MAPNAME
    THINGS
    ...
    so we take the first lump name preceding THINGS.
    */
    const std::vector<Lump> lumps = readDirectory(stream);
    std::string_view previous;
    bool first = true;

    for (const Lump &lump : lumps) {
        if (!first && lumpName(lump) == "THINGS") {
            names.emplace_back(previous);
        }

        previous = lumpName(lump);
        first = false;
    }

    return names;
}

std::string Wad::lump(const std::string_view name) {
    std::ifstream stream(_file, std::ios::binary);

    if (!stream) {
        return {};
    }

    for (const Lump &lump : readDirectory(stream)) {
        if (!Text::iequals(lumpName(lump), name) || lump.length <= 0 || lump.offset < 0) {
            continue;
        }

        std::string bytes(static_cast<size_t>(lump.length), '\0');

        stream.seekg(lump.offset);

        return stream.read(bytes.data(), lump.length) ? bytes : std::string();
    }

    return {};
}

std::vector<std::string> Wad::lumpNames() {
    std::vector<std::string> names;
    std::ifstream stream(_file, std::ios::binary);

    if (!stream) {
        return names;
    }

    for (const Lump &lump : readDirectory(stream)) {
        names.push_back(Text::upper(lumpName(lump)));
    }

    return names;
}

bool Wad::isGame() {
    std::ifstream stream(_file, std::ios::binary);
    Header header{};

    if (!stream || !stream.read(reinterpret_cast<char *>(&header), sizeof(header))) {
        return false;
    }

    // The one letter between a game and a patch on top of one.
    return std::string_view(header.type, sizeof(header.type)) == "IWAD";
}

std::string Wad::iwadinfoName() {
    std::ifstream stream(_file, std::ios::binary);

    if (!stream) {
        return {};
    }

    const std::vector<Lump> lumps = readDirectory(stream);

    for (const Lump &lump : lumps) {
        if (lumpName(lump) != "IWADINFO" || lump.length <= 0 || lump.offset < 0) {
            continue;
        }

        std::string text(static_cast<size_t>(lump.length), '\0');

        stream.seekg(lump.offset);

        if (!stream.read(text.data(), lump.length)) {
            return {};
        }

        return nameFromIwadinfo(text);
    }

    return {};
}

bool Wad::isMapXX() {
    std::ifstream stream(_file, std::ios::binary);

    if (!stream) {
        return false;
    }

    const std::vector<Lump> lumps = readDirectory(stream);

    for (const Lump &lump : lumps) {
        if (lumpName(lump) == "MAP01") {
            return true;
        }
    }

    return false;
}
