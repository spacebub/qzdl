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

#include <array>
#include <span>
#include <utility>

#include "core/config/Schema.h"
#include "core/launch/Flags.h"
#include "core/launch/Rules.h"

namespace Rules {

namespace {

constexpr PortLines UZ = bitOf(PortLine::uzdoom);
constexpr PortLines ZDOOMS =
        bitOf(PortLine::uzdoom) | bitOf(PortLine::gzdoom) | bitOf(PortLine::zdoom) | bitOf(PortLine::zandronum);
constexpr PortLines DSDA = bitOf(PortLine::dsda) | bitOf(PortLine::nyan);
constexpr PortLines NYAN = bitOf(PortLine::nyan);
constexpr PortLines PRBOOM = bitOf(PortLine::prboom);
constexpr PortLines WOOF = bitOf(PortLine::woof);
constexpr PortLines CRISPY = bitOf(PortLine::crispy);
constexpr PortLines VANILLA = bitOf(PortLine::chocolate) | CRISPY;
constexpr PortLines ETERNITY = bitOf(PortLine::eternity);

using enum Rule;
using enum Kind;

constexpr std::array INFO = std::to_array<RuleInfo>({
        {RuleKey::COOP_SPAWNS, "Coop things", toggle, {}},
        {RuleKey::COOP, "Coop preset", toggle, {}},
        {RuleKey::ALWAYS_APPLY_DMFLAGS, "Deathmatch flags everywhere", toggle, {}},
        {RuleKey::CHAIN_EPISODES, "Chain episodes", toggle, {}},
        {RuleKey::DOUBLE_AMMO, "Double ammo", toggle, {}},
        {RuleKey::NO_CHEATS, "Ignore patch cheats", toggle, {}},
        {RuleKey::NO_OPTIONS, "Skip OPTIONS lumps", toggle, {}},
        {RuleKey::LIMIT_REMOVING, "Limit removing", toggle, {}},
        {RuleKey::VANILLA, "Vanilla mode", toggle, {}},
        {RuleKey::BETA, "MBF beta", toggle, {}},
        {RuleKey::STROLLER, "Stroller", toggle, {}},
        {RuleKey::FLIP_LEVELS, "Mirrored maps", toggle, {}},
        {RuleKey::FLIP_WEAPONS, "Mirrored weapons", toggle, {}},
        {RuleKey::TURBO, "Turbo", number, "100"},
        {RuleKey::SPEED, "Game speed", number, "100"},
        {RuleKey::DOGS, "Helper dogs", number, "Port's own"},
});

static_assert(INFO.size() == RULES, "a key and a label for every rule");

// clang-format off
// NOLINTBEGIN(readability-trailing-comma): each row is a line of the table, with its description below it
constexpr std::array SPELLINGS = std::to_array<Spelling>({
        {coopSpawns, DSDA | PRBOOM | WOOF | CRISPY, "-coop_spawns", {}, 0, 0, 0,
         "A single player game places the things meant for a cooperative one."},
        {coopPreset, UZ, "-coop", {}, 0, 0, 0,
         "Sets up a cooperative game in one go. Multiplayer weapons stay out, players pass through each other, "
         "keys are shared, each player picks up a copy of an item and the last weapon is kept. It does nothing in "
         "a deathmatch."},
        {alwaysApplyDmflags, ZDOOMS, "+alwaysapplydmflags", Rules::ON, 0, 0, 0,
         "The dmflags that apply only in deathmatch apply in every game."},
        {chainEpisodes, DSDA, "-chain_episodes", {}, 0, 0, 0,
         "Finishing an episode goes on to the next one without a stop."},
        {doubleAmmo, CRISPY, "-doubleammo", {}, 0, 0, RECORDING | PLAYBACK | NETGAME,
         "Ammo pickups give twice as much. Left out while a demo is recorded or played and in a netgame, since "
         "the port stops on it there."},
        {noCheats, DSDA | PRBOOM | WOOF | VANILLA, "-nocheats", {}, 0, 0, 0,
         "Cheat codes that a DeHackEd patch changes are not applied."},
        {noOptions, WOOF, "-nooptions", {}, 0, 0, 0,
         "OPTIONS lumps in loaded files are not read, so a map cannot change the settings of the port."},
        {limitRemoving, NYAN, "-lr", {}, 0, 0, 0,
         "Vanilla complevels ignore the limits and overflows of the original game."},
        {vanilla, ETERNITY, "-vanilla", {}, 0, 0, 0,
         "Turns on every compatibility option and records demos as Doom 1.9 does. It holds only when the game "
         "starts from the command line, as with a map or a skill picked."},
        {beta, WOOF, "-beta", {}, 0, 0, 0,
         "Plays as the beta of MBF did. Needs betagrph.wad, and goes on only with the MBF complevel picked, since "
         "the port stops at any other.",
         11, 11, true},
        {stroller, DSDA | PRBOOM, "-stroller", {}, 0, 0, 0,
         "Plays by the stroller category rules: half speed and no strafing. Turbo has no say while it is on."},
        {flipLevels, CRISPY, "-fliplevels", {}, 0, 0, 0,
         "Mirrors every map from left to right, weapons included. With Mirrored weapons on as well, the weapons "
         "come out the right way round."},
        {flipWeapons, CRISPY, "-flipweapons", {}, 0, 0, 0,
         "Mirrors the weapon sprites only. With Mirrored maps on as well, the weapons come out the right way round."},
        {turbo, ZDOOMS | DSDA, "-turbo", {}, 10, 255, 0,
         "Player speed in percent, from 10 to 255. 100 is normal."},
        {turbo, PRBOOM | WOOF | VANILLA | ETERNITY, "-turbo", {}, 10, 400, 0,
         "Player speed in percent, from 10 to 400. 100 is normal."},
        {speed, DSDA, "-game_speed", {}, 10, 10000, 0,
         "Game speed in percent, from 10 to 10000. Strict mode keeps it at 100."},
        {speed, WOOF, "-speed", {}, 10, 1000, 0,
         "Game speed in percent, from 10 to 1000. Strict mode and netgames keep it at 100."},
        {speed, ETERNITY, "-speed", {}, 10, 1000, 0,
         "Game speed in percent, from 10 to 1000."},
        {dogs, PRBOOM | ETERNITY, "-dogs", {}, 0, 3, 0,
         "How many helper dogs join a single player game, from 0 to 3."},
        {dogs, WOOF, "-dogs", {}, 0, 3, PLAYBACK,
         "How many helper dogs join a single player game, from 0 to 3. Left out below the MBF complevel, since the "
         "port stops there.",
         11, 99, false},
});
// NOLINTEND(readability-trailing-comma)
// clang-format on

// The row each line spells a rule with, or -1.
constexpr std::array<std::array<int, RULES>, PORT_LINES> SPELLED = [] {
    std::array<std::array<int, RULES>, PORT_LINES> out{};

    for (std::array<int, RULES> &line : out) {
        line.fill(-1);
    }

    for (std::size_t at = 0; at < SPELLINGS.size(); ++at) {
        for (std::size_t line = 0; line < PORT_LINES; ++line) {
            if ((SPELLINGS[at].lines & bitOf(static_cast<PortLine>(line))) != 0) {
                out[line][static_cast<std::size_t>(SPELLINGS[at].rule)] = static_cast<int>(at);
            }
        }
    }

    return out;
}();

}

const RuleInfo &info(const Rule rule) {
    return INFO[static_cast<std::size_t>(rule)];
}

const Spelling *spelling(const PortLine line, const Rule rule) {
    const int at = SPELLED[static_cast<std::size_t>(line)][static_cast<std::size_t>(rule)];

    return at < 0 ? nullptr : &SPELLINGS[static_cast<std::size_t>(at)];
}

std::span<const Spelling> spellings() {
    return SPELLINGS;
}

std::optional<int> parse(const Spelling &spelled, const std::string_view typed) {
    const std::optional<std::uint32_t> parsed = Flags::parse(typed);

    if (!parsed || std::cmp_less(*parsed, spelled.least) || std::cmp_greater(*parsed, spelled.most)) {
        return std::nullopt;
    }

    return static_cast<int>(*parsed);
}

bool worksAt(const Spelling &spelled, const int level) {
    if (spelled.fromLevel < 0) {
        return true;
    }

    if (level < 0) {
        return !spelled.insists;
    }

    return level >= spelled.fromLevel && level <= spelled.toLevel;
}

}
