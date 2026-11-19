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
#include <optional>
#include <span>
#include <string_view>

#include "core/launch/Dialect.h"
#include "core/launch/Port.h"

namespace Port {

enum class Match : std::uint8_t {
    contains,
    whole,

    // The whole name, and never a Flatpak export. No DOS release comes as one.
    wholeOutsideFlatpak,
};

// How a DOS port runs from its folder in the profile.
struct DosRun {
    // The config it reads beside itself. Empty names it after the program, as MBF does.
    std::string_view config{};

    // Data files it opens beside itself, copied in from the port folder when they are there.
    std::array<std::string_view, 2> companions{};

    // Looks for The Ultimate Doom as doomu.wad. Vanilla knows only doom.wad.
    bool doomu{false};

    // -playdemo takes only the bare name of a file in the current folder.
    bool bareDemo{false};
};

struct Known {
    Port port;

    // What zdl.json and the release cache call the port. Empty for a port nobody picks.
    std::string_view id{};

    // Empty for a row of several ports, such as the ZDoom forks.
    std::string_view name{};

    // What the file name holds, lower case and without its extension.
    std::array<std::string_view, 5> words{};

    Match match{Match::contains};

    // What the port speaks. Dialect::of sets the port of the traits from this row.
    const Dialect::Traits *traits;

    // Newest first. Empty for a port without version rules.
    std::span<const Dialect::VersionRange> releases{};

    // The name the port gives its folders and files. Saves are listed only for a port with one.
    std::string_view dataName{};

    // What a numbered save is named before its number.
    std::string_view slotPrefix{};

    // Chocolate and Crispy save Heretic, Hexen and Strife in this folder below savegames, whatever the IWAD. Empty
    // when the IWAD decides.
    std::string_view savesFolder{};

    // Set for a port that runs under DOSBox from a folder in the profile.
    std::optional<DosRun> dos{};
};

[[nodiscard]] std::span<const Known> known();

[[nodiscard]] const Known &known(Port port);

// The port a file runs. A Flatpak export goes by the app it runs.
[[nodiscard]] const Known &of(const std::filesystem::path &file);

// Null for an empty id and for one no port has.
[[nodiscard]] const Known *withId(std::string_view id);

// The port a word in zdl.json or the release cache names. Unknown for an empty word and for one no port has.
[[nodiscard]] Port fromId(std::string_view id);

}
