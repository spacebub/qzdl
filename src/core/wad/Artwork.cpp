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

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <span>
#include <unordered_map>
#include <utility>

#include "core/wad/Artwork.h"
#include "core/wad/MapFile.h"
#include "external/stb/stb_image.h"

namespace {

// A full-screen flat has no header, so its size is what names it.
constexpr int FLAT_WIDTH = 320;
constexpr int FLAT_HEIGHT = 200;
constexpr size_t FLAT_SIZE = static_cast<size_t>(FLAT_WIDTH) * FLAT_HEIGHT;

constexpr size_t PALETTE_SIZE = static_cast<size_t>(256) * 3;

// Share of an expansion's lumps a neighbour must have to count as its base game.
constexpr size_t KINSHIP_NUMERATOR = 2;
constexpr size_t KINSHIP_DENOMINATOR = 5;

// Larger is not a title screen.
constexpr int SIZE_LIMIT = 4096;

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

// Best first. The first TITLE_NAMES are the title screen itself.
constexpr std::array<std::string_view, 5> DRAWN_UNDER = {
        "TITLEPIC", "TITLE", "INTERPIC", "CREDIT", "STARTUP",
};

constexpr size_t TITLE_NAMES = 2;

// Magic bytes, and the suffix the image is read back under.
constexpr std::array<std::pair<std::string_view, std::string_view>, 3> MAGIC = {{
        {"\x89PNG", ".png"},
        {"\xFF\xD8\xFF", ".jpg"},
        {"GIF8", ".gif"},
}};

constexpr std::array<std::string_view, MAGIC.size()> SUFFIXES = {".png", ".jpg", ".gif"};

static_assert(SUFFIXES[0] == MAGIC[0].second);
static_assert(SUFFIXES[1] == MAGIC[1].second);
static_assert(SUFFIXES[2] == MAGIC[2].second);

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

// A flat, or a patch with a plausible header.
bool shaped(const std::string &lump, int &width, int &height) {
    if (lump.size() == FLAT_SIZE) {
        width = FLAT_WIDTH;
        height = FLAT_HEIGHT;

        return true;
    }

    if (lump.size() < 8) {
        return false;
    }

    width = readShort(lump, 0);
    height = readShort(lump, 2);

    return width >= 1 && height >= 1 && width <= SIZE_LIMIT && height <= SIZE_LIMIT
        && lump.size() >= 8 + (static_cast<size_t>(width) * 4);
}

std::vector<std::uint8_t> toRgb(const std::span<const std::uint8_t> indexes,
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

// Column-major: an offset table, then per column a run of posts of
// (top, length, pad, pixels..., pad) ending in POST_END.
std::vector<std::uint8_t> patch(const std::string &bytes, const int width, const int height) {
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

// Lump name to the index of the neighbour that last counted it.
std::unordered_map<std::string, size_t> namesOf(MapFile &map) {
    std::unordered_map<std::string, size_t> names;

    for (std::string &name : map.lumpNames()) {
        names.emplace(std::move(name), 0);
    }

    return names;
}

// The palette of the neighbouring game sharing the most lumps with this file.
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

        if (!other || !other->isGame()) {
            continue;
        }

        size_t shared = 0;

        ++counted;

        // The counter marks names already seen for this neighbour, so no set is needed.
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

Title titleOf(const std::filesystem::path &file, const std::string_view palette,
              const Under under) {
    const std::unique_ptr<MapFile> map = MapFile::open(file);

    if (!map) {
        return {};
    }

    Title title;

    title.lump = map->picture(std::span(DRAWN_UNDER).first(
        under == Under::Title ? TITLE_NAMES : DRAWN_UNDER.size()));

    if (title.lump.empty()) {
        return title;
    }

    title.image = isImageFile(title.lump);

    if (title.image) {
        return title;
    }

    title.palette = map->lump("PLAYPAL");
    title.own = !title.palette.empty();

    if (title.palette.empty()) {
        title.palette = palette.empty() ? borrowed(*map, file) : std::string(palette);
    }

    return title;
}

std::string paletteOf(const std::filesystem::path &file) {
    const std::unique_ptr<MapFile> map = MapFile::open(file);

    return map ? map->lump("PLAYPAL") : std::string();
}

Picture decode(const Title &title) {
    int width = 0;
    int height = 0;

    // Rejects oversized pictures before anything is allocated.
    measure(title, width, height);

    if (width == 0) {
        return {};
    }

    if (title.image) {
        int had = 0;

        stbi_uc *pixels = stbi_load_from_memory(
            reinterpret_cast<const stbi_uc *>(title.lump.data()),
            static_cast<int>(title.lump.size()), &width, &height, &had, 3);

        if (pixels == nullptr) {
            return {};
        }

        Picture made{.width = width,
                     .height = height,
                     .pixels = std::vector<std::uint8_t>(
                         pixels, pixels + (static_cast<size_t>(width) * height * 3))};

        stbi_image_free(pixels);

        return made;
    }

    if (title.palette.size() < PALETTE_SIZE) {
        return {};
    }

    if (title.lump.size() == FLAT_SIZE) {
        return {.width = FLAT_WIDTH,
                .height = FLAT_HEIGHT,
                .pixels = toRgb(std::span(reinterpret_cast<const std::uint8_t *>(
                                               title.lump.data()), title.lump.size()),
                                 title.palette)};
    }

    const std::vector<std::uint8_t> indexes = patch(title.lump, width, height);

    if (indexes.empty()) {
        return {};
    }

    return {.width = width,
            .height = height,
            .pixels = toRgb(indexes, title.palette)};
}

std::string_view suffixOf(const Title &title) {
    return suffixFor(title.lump);
}

void measure(const Title &title, int &width, int &height) {
    width = 0;
    height = 0;

    if (title.empty()) {
        return;
    }

    int had = 0;
    const bool read = title.image
        ? stbi_info_from_memory(reinterpret_cast<const stbi_uc *>(title.lump.data()),
                                static_cast<int>(title.lump.size()), &width, &height, &had) != 0
        : shaped(title.lump, width, height);

    if (!read || width < 1 || height < 1 || width > SIZE_LIMIT || height > SIZE_LIMIT) {
        width = 0;
        height = 0;
    }
}

std::span<const std::string_view> suffixes() {
    return SUFFIXES;
}

}
