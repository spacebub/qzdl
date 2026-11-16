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
#include <filesystem>
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
    skulltag,
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

    // The whole name, and never a Flatpak export. No DOS release comes as one.
    wholeOutsideFlatpak,
};

struct Known {
    Program program;

    // What zdl.json stores for a downloaded port. Empty for a program the download list lacks.
    std::string_view portId{};

    // Empty for a row of several programs, such as Chocolate Heretic, Hexen and Strife.
    std::string_view name{};

    // What the file name holds, lower case and without its extension.
    std::array<std::string_view, 5> words{};

    Match match{Match::contains};

    // What the program speaks. Dialect::of sets the program of the port from this row.
    const Dialect::Port *port;

    // Newest first. Empty for a program without version rules.
    std::span<const Dialect::VersionRange> releases{};

    // The name the program gives its folders and files. Saves are listed only for a program with one.
    std::string_view dataName{};

    // What a numbered save is named before its number.
    std::string_view slotPrefix{};
};

[[nodiscard]] std::span<const Known> known();

[[nodiscard]] const Known &known(Program program);

// The program a file runs. A Flatpak export goes by the app it runs.
[[nodiscard]] const Known &of(const std::filesystem::path &file);

}
