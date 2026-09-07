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

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <span>
#include <unordered_map>
#include <utility>

#include "core/Artwork.h"
#include "core/MapFile.h"

namespace {

// The screen every one of these games is 320x200, and a flat of one is stored
// with no header at all, so the size is the only thing that names it.
constexpr int FLAT_WIDTH = 320;
constexpr int FLAT_HEIGHT = 200;
constexpr size_t FLAT_SIZE = static_cast<size_t>(FLAT_WIDTH) * FLAT_HEIGHT;

constexpr size_t PALETTE_SIZE = static_cast<size_t>(256) * 3;

/*
How much of an expansion a game has to account for before its colours are
taken to be the ones the expansion was drawn against. Anything less is a
neighbour in the same folder rather than the game underneath.
*/
constexpr size_t KINSHIP_NUMERATOR = 2;
constexpr size_t KINSHIP_DENOMINATOR = 5;

// A patch that claims more than this is not a title screen, whatever else it
// may be.
constexpr int SIZE_LIMIT = 4096;

/*
A post says nothing about the run before it, so a column that is drawn on
twice is one this cannot straighten out; it is written last-wins, as the
renderer does.
*/
constexpr std::uint8_t POST_END = 0xFF;

std::int16_t readShort(const std::string &bytes, const size_t at) {
    std::int16_t value = 0;

    std::memcpy(&value, bytes.data() + at, sizeof(value));

    return value;
}

std::int32_t readLong(const std::string &bytes, const size_t at) {
    std::int32_t value = 0;

    std::memcpy(&value, bytes.data() + at, sizeof(value));

    return value;
}

// The bytes a picture that names its own colours opens with, and the name it
// is read back under.
constexpr std::array<std::pair<std::string_view, std::string_view>, 3> MAGIC = {{
        {"\x89PNG", ".png"},
        {"\xFF\xD8\xFF", ".jpg"},
        {"GIF8", ".gif"},
}};

std::string_view suffixFor(const std::string &bytes) {
    for (const auto &[magic, suffix] : MAGIC) {
        if (bytes.starts_with(magic)) {
            return suffix;
        }
    }

    return {};
}

bool isImageFile(const std::string &bytes) {
    return !suffixFor(bytes).empty();
}

// Indexes into the palette, laid out row by row. Anything left over is black.
std::vector<std::uint8_t> colour(const std::span<const std::uint8_t> indexes,
                                 const std::string &palette) {
    std::vector<std::uint8_t> pixels(indexes.size() * 3, 0);

    for (size_t at = 0; at < indexes.size(); ++at) {
        const size_t entry = static_cast<size_t>(indexes[at]) * 3;

        pixels[at * 3] = static_cast<std::uint8_t>(palette[entry]);
        pixels[(at * 3) + 1] = static_cast<std::uint8_t>(palette[entry + 1]);
        pixels[(at * 3) + 2] = static_cast<std::uint8_t>(palette[entry + 2]);
    }

    return pixels;
}

/*
A patch is stored down its columns rather than across its rows: a table of
offsets, one to a column, and at each of those a run of posts saying how far
down the run starts, how long it is, and then that many pixels between a pair
of padding bytes.
*/
std::vector<std::uint8_t> patch(const std::string &bytes, const int width, const int height) {
    // 0 is what the games use for the black a title screen is all of anyway,
    // which is also what an untouched pixel should come out as.
    std::vector<std::uint8_t> indexes(static_cast<size_t>(width) * height, 0);
    const size_t size = bytes.size();

    for (int column = 0; column < width; ++column) {
        const std::int32_t start = readLong(bytes, 8 + (static_cast<size_t>(column) * 4));

        if (start < 0 || std::cmp_greater_equal(start, size)) {
            return {};
        }

        auto at = static_cast<size_t>(start);

        while (at < size) {
            const auto top = static_cast<std::uint8_t>(bytes[at]);

            if (top == POST_END) {
                break;
            }

            if (at + 2 >= size) {
                return {};
            }

            const auto length = static_cast<size_t>(static_cast<std::uint8_t>(bytes[at + 1]));

            // The pixels sit between a leading and a trailing padding byte.
            if (at + 3 + length + 1 > size) {
                return {};
            }

            for (size_t step = 0; step < length; ++step) {
                if (const size_t row = top + step; std::cmp_less(row, height)) {
                    indexes[(row * width) + column] = static_cast<std::uint8_t>(bytes[at + 3 + step]);
                }
            }

            at += 4 + length;
        }
    }

    return indexes;
}

// Every name the file holds, against the neighbour that last counted it: a WAD
// names its map lumps over and over, and each is one name in common.
std::unordered_map<std::string, size_t> namesOf(MapFile &map) {
    std::unordered_map<std::string, size_t> names;

    for (std::string &name : map.lumpNames()) {
        names.emplace(std::move(name), 0);
    }

    return names;
}

/*
An expansion ships the picture and leaves the colours to the game it was made
to sit on top of. Which game that is is written down nowhere, but it shows:
the two share the lumps the expansion was built out of, and nothing else in
the folder it sits in comes close.
*/
std::string borrowed(MapFile &map, const std::filesystem::path &file) {
    std::unordered_map<std::string, size_t> own = namesOf(map);

    if (own.empty()) {
        return {};
    }

    std::error_code code;
    std::filesystem::path game;
    size_t most = 0;
    size_t counted = 0;

    for (const auto &entry : std::filesystem::directory_iterator(file.parent_path(), code)) {
        if (!entry.is_regular_file(code) || entry.path() == file) {
            continue;
        }

        const std::unique_ptr<MapFile> other = MapFile::open(entry.path());

        /*
        Only a game, and only one carrying the colours: a neighbour built on
        the same game shares plenty of lumps and none of the authority, and
        its own palette is as likely as not to be a tinted one.
        */
        if (!other || !other->isGame()) {
            continue;
        }

        size_t shared = 0;

        ++counted;

        // Counted against this neighbour's number rather than into a set of
        // its own, so nothing is allocated to compare one list with another.
        for (const std::string &name : other->lumpNames()) {
            if (const auto found = own.find(name);
                found != own.end() && found->second != counted) {
                found->second = counted;
                ++shared;
            }
        }

        if (shared > most) {
            most = shared;
            game = entry.path();
        }
    }

    if (game.empty() || most * KINSHIP_DENOMINATOR < own.size() * KINSHIP_NUMERATOR) {
        return {};
    }

    const std::unique_ptr<MapFile> base = MapFile::open(game);

    return base ? base->lump("PLAYPAL") : std::string();
}

}

namespace Artwork {

Title titleOf(const std::filesystem::path &file) {
    const std::unique_ptr<MapFile> map = MapFile::open(file);

    if (!map) {
        return {};
    }

    Title title;

    title.lump = map->lump("TITLEPIC");

    if (title.lump.empty()) {
        title.lump = map->lump("TITLE");
    }

    if (title.lump.empty()) {
        return title;
    }

    title.image = isImageFile(title.lump);

    if (title.image) {
        return title;
    }

    title.palette = map->lump("PLAYPAL");

    if (title.palette.empty()) {
        title.palette = borrowed(*map, file);
    }

    return title;
}

Picture decode(const Title &title) {
    if (title.palette.size() < PALETTE_SIZE) {
        return {};
    }

    // A flat is already the indexes, row by row, so it is read where it lies.
    if (title.lump.size() == FLAT_SIZE) {
        return {.width = FLAT_WIDTH,
                .height = FLAT_HEIGHT,
                .pixels = colour(std::span(reinterpret_cast<const std::uint8_t *>(
                                               title.lump.data()), title.lump.size()),
                                 title.palette)};
    }

    if (title.lump.size() < 8) {
        return {};
    }

    const int width = readShort(title.lump, 0);
    const int height = readShort(title.lump, 2);

    if (width < 1 || height < 1 || width > SIZE_LIMIT || height > SIZE_LIMIT
        || title.lump.size() < 8 + (static_cast<size_t>(width) * 4)) {
        return {};
    }

    const std::vector<std::uint8_t> indexes = patch(title.lump, width, height);

    if (indexes.empty()) {
        return {};
    }

    return {.width = width,
            .height = height,
            .pixels = colour(indexes, title.palette)};
}

std::string_view suffixOf(const Title &title) {
    return suffixFor(title.lump);
}

}
