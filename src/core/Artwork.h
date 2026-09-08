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
#include <span>
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

/*
Which names a file is looked for a picture under. Title is the screen the port
opens on and nothing else; Any takes in what a mod carrying no title screen
still draws itself under, and is only worth asking when nothing carries one.
*/
enum class Under : std::uint8_t { Title, Any };

// A title screen as it sits in the file, and the colours to read it with.
struct Title {
    std::string lump;

    // 768 bytes, three to a colour. Empty when the file has none of its own,
    // which is what an expansion loaded over a base game leaves behind.
    std::string palette;

    // A picture in a format that names its own colours, and so is nobody
    // else's to read. What a PK3 storing its graphics as PNG has.
    bool image{false};

    // Whether the colours above came out of the file itself rather than a game
    // underneath it. What is kept of one drawn against its own is anyone's.
    bool own{false};

    [[nodiscard]] bool empty() const { return lump.empty(); }
};

// Straight RGB, three bytes a pixel, the top row first.
struct Picture {
    int width{0};
    int height{0};
    std::vector<std::uint8_t> pixels;

    [[nodiscard]] bool empty() const { return pixels.empty(); }
};

/*
An expansion ships the picture and leaves the colours to the game it is loaded
over, and only the caller knows which game that is. Handing them in says so;
handing in nothing leaves the file's neighbours to be guessed at instead.
*/
[[nodiscard]] Title titleOf(const std::filesystem::path &file, std::string_view palette = {},
                            Under under = Under::Any);

// The colours a game hands whatever is loaded on top of it. Empty for a file
// carrying none of its own.
[[nodiscard]] std::string paletteOf(const std::filesystem::path &file);

/*
A picture that names its own colours is read as it stands. One that does not is
only worth reading if the lump is one of the two shapes Doom stores a full
screen in: a 320x200 flat, or a column-major patch.
*/
[[nodiscard]] Picture decode(const Title &title);

// What a picture of its own kind would come out as, without reading the whole
// of it. Both zero for anything that cannot be read, or is too big to be one.
void measure(const Title &title, int &width, int &height);

// The name whoever goes by names rather than bytes expects the lump under.
// Empty unless it is a picture in its own right.
[[nodiscard]] std::string_view suffixOf(const Title &title);

// Every name one of those can go under, for whoever has to look for a picture
// already put aside without opening the file it came out of.
[[nodiscard]] std::span<const std::string_view> suffixes();

}
