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
#include <unordered_set>
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

bool isImageFile(const std::string &bytes) {
    static constexpr std::array<std::string_view, 3> MAGIC = {
            "\x89PNG", "\xFF\xD8\xFF", "GIF8",
    };

    return std::ranges::any_of(MAGIC, [&bytes](const std::string_view magic) {
        return bytes.starts_with(magic);
    });
}

// Indexes into the palette, laid out row by row. Anything left over is black.
std::vector<std::uint8_t> colour(const std::vector<std::uint8_t> &indexes,
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

std::unordered_set<std::string> namesOf(MapFile &map) {
    std::vector<std::string> names = map.lumpNames();

    return {std::make_move_iterator(names.begin()), std::make_move_iterator(names.end())};
}

/*
An expansion ships the picture and leaves the colours to the game it was made
to sit on top of. Which game that is is written down nowhere, but it shows:
the two share the lumps the expansion was built out of, and nothing else in
the folder it sits in comes close.
*/
std::string borrowed(MapFile &map, const std::filesystem::path &file) {
    const std::unordered_set<std::string> own = namesOf(map);

    if (own.empty()) {
        return {};
    }

    std::error_code code;
    std::filesystem::path game;
    size_t most = 0;

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

        for (const std::string &name : namesOf(*other)) {
            if (own.contains(name)) {
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

    if (title.lump.size() == FLAT_SIZE) {
        const std::vector<std::uint8_t> indexes(title.lump.begin(), title.lump.end());

        return {.width = FLAT_WIDTH,
                .height = FLAT_HEIGHT,
                .pixels = colour(indexes, title.palette)};
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

}
