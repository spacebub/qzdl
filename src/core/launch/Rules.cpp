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
#include <cstdint>
#include <limits>
#include <span>
#include <utility>

#include "core/launch/Dialect.h"
#include "core/launch/Flags.h"
#include "core/launch/Rules.h"

namespace Rules {

namespace {

using namespace Lines;
using enum Rule;
using enum Kind;

constexpr Range NO_RANGE{};
constexpr std::uint32_t NEVER = 0;
constexpr Levels ANY_LEVEL{};
constexpr int AND_UP = std::numeric_limits<int>::max();

// clang-format off
constexpr std::array<RuleInfo, RULES> INFO = std::to_array<RuleInfo>({
        {coopSpawns,         "Coop things",                 {},    toggle},
        {coopPreset,         "Coop preset",                 {},    toggle},
        {alwaysApplyDmflags, "Deathmatch flags everywhere", {},    toggle},
        {chainEpisodes,      "Chain episodes",              {},    toggle},
        {doubleAmmo,         "Double ammo",                 {},    toggle},
        {noCheats,           "Ignore patch cheats",         {},    toggle},
        {noOptions,          "Skip OPTIONS lumps",          {},    toggle},
        {limitRemoving,      "Limit removing",              {},    toggle},
        {vanilla,            "Vanilla mode",                {},    toggle},
        {beta,               "MBF beta",                    {},    toggle},
        {stroller,           "Stroller",                    {},    toggle},
        {flipLevels,         "Mirrored maps",               {},    toggle},
        {flipWeapons,        "Mirrored weapons",            {},    toggle},
        {turbo,              "Turbo",                       "100", number},
        {speed,              "Game speed",                  "100", number},
        {dogs,               "Helper dogs",                 {},    number},
});
// clang-format on

static_assert(std::ranges::all_of(ALL, [](const Rule rule) { return INFO[index(rule)].rule == rule; }),
              "INFO in Rule order");
static_assert(std::ranges::all_of(ALL,
                                  [](const Rule rule) {
                                      return (INFO[index(rule)].kind == number) == takesNumber(rule);
                                  }),
              "the rules that take a number come last");

// clang-format off
// NOLINTBEGIN(readability-trailing-comma): each row is a line of the table, with its description below it
constexpr std::array SPELLINGS = std::to_array<Spelling>({
        {coopSpawns,         DSDA_LINE | PRBOOM | WOOF | CRISPY,      "-coop_spawns",
         "A single player game places the things meant for a cooperative one."},
        {coopPreset,         UZDOOM,                                  "-coop",
         "Sets up a cooperative game in one go. Multiplayer weapons stay out, players pass through each other, "
         "keys are shared, each player picks up a copy of an item and the last weapon is kept. It does nothing in "
         "a deathmatch."},
        {alwaysApplyDmflags, ZDOOM_FAMILY,                            "+alwaysapplydmflags",
         "The dmflags that apply only in deathmatch apply in every game.",
         NO_RANGE, NEVER, ANY_LEVEL, "1"},
        {chainEpisodes,      DSDA_LINE,                               "-chain_episodes",
         "Finishing an episode goes on to the next one without a stop."},
        {doubleAmmo,         CRISPY,                                  "-doubleammo",
         "Ammo pickups give twice as much. Left out while a demo is recorded or played and in a netgame, since "
         "the port stops on it there.",
         NO_RANGE, RECORDING | PLAYBACK | NETGAME},
        {noCheats,           DSDA_LINE | PRBOOM | WOOF | VANILLA_LINE, "-nocheats",
         "Cheat codes that a DeHackEd patch changes are not applied."},
        {noOptions,          WOOF,                                    "-nooptions",
         "OPTIONS lumps in loaded files are not read, so a map cannot change the settings of the port."},
        {limitRemoving,      NYAN,                                    "-lr",
         "Vanilla complevels ignore the limits and overflows of the original game."},
        {vanilla,            ETERNITY,                                "-vanilla",
         "Turns on every compatibility option and records demos as Doom 1.9 does. It holds only when the game "
         "starts from the command line, as with a map or a skill picked."},
        {beta,               WOOF,                                    "-beta",
         "Plays as the beta of MBF did. Needs betagrph.wad, and goes on only with the MBF complevel picked, since "
         "the port stops at any other.",
         NO_RANGE, NEVER, Levels{.from = Dialect::MBF, .to = Dialect::MBF, .onLine = true}},
        {stroller,           DSDA_LINE | PRBOOM,                      "-stroller",
         "Plays by the stroller category rules: half speed and no strafing. Turbo has no say while it is on."},
        {flipLevels,         CRISPY,                                  "-fliplevels",
         "Mirrors every map from left to right, weapons included. With Mirrored weapons on as well, the weapons "
         "come out the right way round."},
        {flipWeapons,        CRISPY,                                  "-flipweapons",
         "Mirrors the weapon sprites only. With Mirrored maps on as well, the weapons come out the right way round."},
        {turbo,              ZDOOM_FAMILY | DSDA_LINE,                "-turbo",
         "Player speed in percent, from 10 to 255. 100 is normal.",
         Range{.least = 10, .most = 255}},
        {turbo,              PRBOOM | WOOF | VANILLA_LINE | ETERNITY, "-turbo",
         "Player speed in percent, from 10 to 400. 100 is normal.",
         Range{.least = 10, .most = 400}},
        {speed,              DSDA_LINE,                               "-game_speed",
         "Game speed in percent, from 10 to 10000. Strict mode keeps it at 100.",
         Range{.least = 10, .most = 10000}},
        {speed,              WOOF,                                    "-speed",
         "Game speed in percent, from 10 to 1000. Strict mode and netgames keep it at 100.",
         Range{.least = 10, .most = 1000}},
        {speed,              ETERNITY,                                "-speed",
         "Game speed in percent, from 10 to 1000.",
         Range{.least = 10, .most = 1000}},
        {dogs,               PRBOOM | ETERNITY,                       "-dogs",
         "How many helper dogs join a single player game, from 0 to 3.",
         Range{.least = 0, .most = 3}},
        {dogs,               WOOF,                                    "-dogs",
         "How many helper dogs join a single player game, from 0 to 3. Left out below the MBF complevel, since the "
         "port stops there.",
         Range{.least = 0, .most = 3}, PLAYBACK, Levels{.from = Dialect::MBF, .to = AND_UP, .onLine = false}},
});
// NOLINTEND(readability-trailing-comma)
// clang-format on

constexpr std::uint8_t NO_ROW = 0xFF;

static_assert(SPELLINGS.size() < NO_ROW, "every row has an index");

// The row each line spells a rule with, or NO_ROW.
constexpr std::array<std::array<std::uint8_t, RULES>, PORT_LINES> ROW_OF = [] {
    std::array<std::array<std::uint8_t, RULES>, PORT_LINES> out{};

    for (std::array<std::uint8_t, RULES> &line : out) {
        line.fill(NO_ROW);
    }

    for (std::size_t at = 0; at < SPELLINGS.size(); ++at) {
        for (std::size_t line = 0; line < PORT_LINES; ++line) {
            if (has(SPELLINGS[at].lines, static_cast<PortLine>(line))) {
                out[line][index(SPELLINGS[at].rule)] = static_cast<std::uint8_t>(at);
            }
        }
    }

    return out;
}();

}

const RuleInfo &info(const Rule rule) {
    return INFO[index(rule)];
}

const Spelling *spelling(const PortLine line, const Rule rule) {
    const std::uint8_t at = ROW_OF[static_cast<std::size_t>(line)][index(rule)];

    return at == NO_ROW ? nullptr : &SPELLINGS[at];
}

std::span<const Spelling> spellings() {
    return SPELLINGS;
}

std::optional<int> parse(const Spelling &spelled, const std::string_view typed) {
    const std::optional<std::uint32_t> parsed = Flags::parse(typed);

    if (!parsed || std::cmp_less(*parsed, spelled.range.least) || std::cmp_greater(*parsed, spelled.range.most)) {
        return std::nullopt;
    }

    return static_cast<int>(*parsed);
}

bool worksAt(const Spelling &spelled, const int level) {
    const Levels &needs = spelled.levels;

    if (needs.from < 0) {
        return true;
    }

    return level < 0 ? !needs.onLine : level >= needs.from && level <= needs.to;
}

}
