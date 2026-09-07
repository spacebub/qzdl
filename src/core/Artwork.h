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
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

/*
Every game carries the picture it opens on, so nothing has to ship artwork for
games it cannot know about: the title screen is read back out of the IWAD the
library already points at. Doom and its kin call it TITLEPIC, Heretic and Hexen
call it TITLE, and the two are not stored the same way.
*/
namespace Artwork {

// A title screen as it sits in the file, and the colours to read it with.
struct Title {
    std::string lump;

    // 768 bytes, three to a colour. Empty when the file has none of its own,
    // which is what an expansion loaded over a base game leaves behind.
    std::string palette;

    // A picture in a format that names its own colours, and so is nobody
    // else's to read. What a PK3 storing its graphics as PNG has.
    bool image{false};

    [[nodiscard]] bool empty() const { return lump.empty(); }
};

// Straight RGB, three bytes a pixel, the top row first.
struct Picture {
    int width{0};
    int height{0};
    std::vector<std::uint8_t> pixels;

    [[nodiscard]] bool empty() const { return pixels.empty(); }
};

[[nodiscard]] Title titleOf(const std::filesystem::path &file);

/*
Nothing unless the lump is one of the two shapes Doom stores a full screen in: a
320x200 flat, or a column-major patch. Anything else is an image file in its own
right and is left to whoever can read one.
*/
[[nodiscard]] Picture decode(const Title &title);

// The name whoever goes by names rather than bytes expects the lump under.
// Empty unless it is a picture in its own right.
[[nodiscard]] std::string_view suffixOf(const Title &title);

}
