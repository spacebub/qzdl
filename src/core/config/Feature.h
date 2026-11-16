/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
 * Copyright (C) 2018-2019  Lcferrum
 * Copyright (C) 2023-2026  spacebub
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

namespace Dialect {

// What a port takes, as far as it is a yes or no. The form of a capability that comes in several
// is a value in Port, and its bit says whether the port has it at all.
enum class Feature : std::uint8_t {
    // Runs inside DOSBox. Only the entry and Config overloads of of() can tell.
    dos,

    // Clear when the name matched nothing and these are guesses.
    recognised,

    // Pre-Boom ports find the game through $DOOMWADDIR instead.
    iwad,

    // Chocolate Doom's second config file.
    extraConfig,

    // Takes -config. A port without it keeps whatever config it writes on its own.
    configFile,

    // ZDoom's own.
    exec,
    map,

    // -warp takes a two digit map after the episode. The ZDoom family and Chocolate read one digit.
    longWarp,

    respawn,

    // Reads -loadgame, in the form Port::loads names.
    loadgame,

    // Without a save folder the saves cannot be listed.
    saveFolder,

    // Every port has -record and -playdemo. The rest varies.
    demos,
    timedemo,
    fastdemo,

    // Stops on -record without -complevel. DSDA-Doom also takes the level a COMPLVL lump names.
    recordsWithComplevel,

    // Stops on -record without -skill. Continuing a demo with -recordfromto needs none.
    recordsWithSkill,

    // Stops on -recordfromto without -skill as well.
    continuesWithSkill,

    longtics,
    soloNet,

    // -record adds .lmp to the name it is given. Helion writes the exact path.
    recordAddsLmp,

    // -recordfromto. Some also insist on -complevel, which the demo header then overrides.
    continues,
    continueComplevel,

    // Writes levelstat.txt with a time per level.
    levelstat,

    pistolStart,

    // +sv_cheats. The rest allow cheats in single player anyway.
    cheats,

    nomusic,

    // The port saves the display choice, so it outlives the launch.
    displayKept,

    shotdir,

    // The netgame features follow from Port::netgame, except the three marked.
    hosts,
    joins,

    // ZDoom family only.
    players,

    listing,

    // ZDoom family cvars. Without the cvar the time limit goes as -timer.
    fragLimit,
    timeLimitCvar,
    savegame,

    extratic,
    dup,

    // UZDoom dropped -netmode.
    netmode,

    // Deathmatch 3.0, with weapons that stay and items that respawn.
    deathmatch3,

    // ZDoom family userinfo: name, color and team.
    player,
    teams,

    // +team, which Zandronum takes only once a client is in the game.
    teamPick,

    // In a netgame UZDoom reads saves only from its own netgame folder and ignores -savedir.
    netgameFolder,

    coopSpawns,
    coopPreset,
    alwaysApplyDmflags,
    chainEpisodes,
    doubleAmmo,
    noCheats,
    noOptions,
    limitRemoving,
    vanillaMode,
    mbfBeta,
    stroller,
    flipLevels,
    flipWeapons,
    turbo,
    gameSpeed,
    dogs,

    count,
};

static_assert(static_cast<unsigned>(Feature::count) <= 64, "the features fit one 64 bit mask");

struct Features {
    std::uint64_t bits{0};

    constexpr Features() = default;

    // NOLINTNEXTLINE(misc-explicit-constructor): a lone feature stands for a mask in the rows and in a | b.
    constexpr Features(const Feature one) : bits(std::uint64_t{1} << static_cast<unsigned>(one)) {}

    [[nodiscard]] constexpr bool has(const Feature one) const { return (bits & Features(one).bits) != 0; }

    constexpr Features &operator|=(const Features other) {
        bits |= other.bits;

        return *this;
    }

    constexpr Features &operator&=(const Features other) {
        bits &= other.bits;

        return *this;
    }

    friend constexpr bool operator==(const Features &, const Features &) = default;
};

[[nodiscard]] constexpr Features operator|(Features left, const Features right) {
    return left |= right;
}

[[nodiscard]] constexpr Features operator|(const Feature left, const Feature right) {
    return Features(left) | Features(right);
}

[[nodiscard]] constexpr Features operator&(Features left, const Features right) {
    return left &= right;
}

[[nodiscard]] constexpr Features operator~(const Features all) {
    Features out;
    out.bits = ~all.bits;

    return out;
}

}
