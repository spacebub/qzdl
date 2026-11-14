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

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "core/launch/PortLine.h"

namespace Rules {

// Switches that change play beyond the flag words, in the order the Gameplay section shows them: the ones that
// turn on, then the ones that take a number.
enum class Rule : std::uint8_t {
    coopSpawns,
    coopPreset,
    alwaysApplyDmflags,
    chainEpisodes,
    doubleAmmo,
    noCheats,
    noOptions,
    limitRemoving,
    vanilla,
    beta,
    stroller,
    flipLevels,
    flipWeapons,
    turbo,
    speed,
    dogs,
};

inline constexpr std::size_t RULES = static_cast<std::size_t>(Rule::dogs) + 1;

enum class Kind : std::uint8_t {
    toggle,
    number,
};

// Where a port stops on a switch, so the launch leaves it out there.
inline constexpr std::uint32_t RECORDING = 1U;
inline constexpr std::uint32_t PLAYBACK = 2U;
inline constexpr std::uint32_t NETGAME = 4U;

struct RuleInfo {
    // The key the profile keeps it under.
    std::string_view key;
    std::string_view label;
    Kind kind;

    // What an empty number field shows.
    std::string_view placeholder;
};

// A rule as one or more lines spell it.
struct Spelling {
    Rule rule;
    PortLines lines;

    // The switch itself, such as -turbo.
    std::string_view name;

    // What follows a switch that turns on, as +alwaysapplydmflags 1 needs. Empty for none.
    std::string_view value;

    // The range a number has to be in.
    int least;
    int most;

    std::uint32_t refused;
    std::string_view description;

    // The complevels the port takes it at, -1 for any. Without one on the line the port's own counts, unless
    // the switch insists, as when the default of the port does not qualify.
    int fromLevel{-1};
    int toLevel{-1};
    bool insists{false};
};

[[nodiscard]] const RuleInfo &info(Rule rule);

// Null when the line has no such switch.
[[nodiscard]] const Spelling *spelling(PortLine line, Rule rule);

[[nodiscard]] std::span<const Spelling> spellings();

// What a toggle keeps when it is on.
inline constexpr std::string_view ON = "1";

// A whole decimal number within the range of the spelling. Empty text is not one.
[[nodiscard]] std::optional<int> parse(const Spelling &spelled, std::string_view typed);

// Whether the port takes the switch with `level` on the line, -1 for none.
[[nodiscard]] bool worksAt(const Spelling &spelled, int level);

}
