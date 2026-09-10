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

namespace Artwork {

// Title: the title screen only. Any: also what a mod without one draws itself under.
enum class Under : std::uint8_t { Title, Any };

struct Title {
    std::string lump;

    // 768 bytes; empty when the file has none of its own.
    std::string palette;

    // PNG, JPG or GIF, which carries its own colours.
    bool image{false};

    // Whether palette came out of this file rather than a game underneath it.
    bool own{false};

    [[nodiscard]] bool empty() const { return lump.empty(); }
};

// RGB, top row first.
struct Picture {
    int width{0};
    int height{0};
    std::vector<std::uint8_t> pixels;

    [[nodiscard]] bool empty() const { return pixels.empty(); }
};

// Pass the base game's palette for an expansion; otherwise its neighbours are guessed at.
[[nodiscard]] Title titleOf(const std::filesystem::path &file, std::string_view palette = {},
                            Under under = Under::Any);

[[nodiscard]] std::string paletteOf(const std::filesystem::path &file);

[[nodiscard]] Picture decode(const Title &title);

// Both zero when unreadable or too big.
void measure(const Title &title, int &width, int &height);

// Empty unless the lump is an image file.
[[nodiscard]] std::string_view suffixOf(const Title &title);

[[nodiscard]] std::span<const std::string_view> suffixes();

}
