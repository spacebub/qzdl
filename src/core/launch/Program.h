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

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include "core/launch/Dialect.h"

namespace Program {

// In the order a file name is matched, most specific first.
enum class Program : std::uint8_t {
    unknown,
    doom,
    legacy,
    helion,
    zdoom,
    zandronum,
    uzdoom,
    gzdoom,
    vkdoom,

    // Any other name with zdoom in it, such as LZDoom or QZDoom. It speaks GZDoom.
    zdoomFork,

    // Heretic, Hexen or Strife from the Crispy or Chocolate packages.
    crispyOther,
    crispy,
    chocolateOther,
    chocolate,

    boom,
    mbf,
    eternity,
    woof,
    nugget,
    cherry,
    dsda,
    nyan,
    prboomPlus,
    glboomPlus,
    prboom,
    glboom,
    rude,
};

static_assert(Program{} == Program::unknown, "a port the table does not name is unknown");

enum class Match : std::uint8_t {
    contains,
    whole,

    // The whole name, of a program that is not a Flatpak export. No DOS release comes as one.
    wholeOutsideFlatpak,
};

// How the saves of a program are found and told apart.
enum class Listing : std::uint8_t {
    none,

    // A zip that names its game in info.json.
    gzdoom,

    // A PNG that names its game in a text chunk.
    zdoom,

    helion,
    prboom,
    woof,
    dsda,
    chocolate,
};

struct Saves {
    Listing listing{Listing::none};

    // GAMENAME for a ZDoom port, which names its folders and, lowercased, its ini. The data folder for the others.
    std::string_view folder{};

    // What a numbered save is named before its number.
    std::string_view slotPrefix{};
};

struct Known {
    Program program;

    // The port id a downloaded port stores, and the name the download list shows. Empty for a program the
    // download list lacks.
    std::string_view word{};
    std::string_view name{};

    // What the file name holds, lower case and without its extension.
    std::array<std::string_view, 5> words{};

    Match match{Match::contains};

    // What the program speaks. Dialect::of sets the program of the port from this row.
    const Dialect::Port *port;

    // Newest first. Empty for a program without version rules.
    std::span<const Dialect::VersionRange> releases{};

    Saves saves{};
};

[[nodiscard]] std::span<const Known> known();

[[nodiscard]] const Known &known(Program program);

// The first row whose words the name holds, or unknown.
[[nodiscard]] const Known &named(std::string_view name, bool exported);

}
