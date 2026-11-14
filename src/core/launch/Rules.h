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

#include "core/config/GameplayKeys.h"
#include "core/launch/PortLine.h"

namespace Rules {

enum class Kind : std::uint8_t {
    toggle,
    number,
};

// Where a port stops on a switch, so the launch leaves it out there.
inline constexpr std::uint32_t RECORDING = 1U;
inline constexpr std::uint32_t PLAYBACK = 2U;
inline constexpr std::uint32_t NETGAME = 4U;

// Its key is Rules::key().
struct RuleInfo {
    Rule rule;
    std::string_view label;

    // What an empty number field shows. Empty for the port's own.
    std::string_view placeholder;

    Kind kind;
};

struct Range {
    int least{0};
    int most{0};
};

// The complevels a port takes a switch at, -1 for any.
struct Levels {
    int from{-1};
    int to{-1};

    // The level has to be on the line, since the default of the port does not qualify.
    bool onLine{false};
};

// A rule as one or more lines spell it.
struct Spelling {
    Rule rule;
    PortLines lines;

    // The switch itself, such as -turbo.
    std::string_view name;

    std::string_view description;
    Range range{};
    std::uint32_t refused{0};
    Levels levels{};

    // What follows a switch that turns on, as +alwaysapplydmflags 1 needs. Empty for none.
    std::string_view value{};
};

[[nodiscard]] const RuleInfo &info(Rule rule);

// Null when the line has no such switch.
[[nodiscard]] const Spelling *spelling(PortLine line, Rule rule);

[[nodiscard]] std::span<const Spelling> spellings();

// A whole decimal number within the range of the spelling. Empty text is not one.
[[nodiscard]] std::optional<int> parse(const Spelling &spelled, std::string_view typed);

// Whether the port takes the switch with `level` on the line, -1 for none.
[[nodiscard]] bool worksAt(const Spelling &spelled, int level);

}
