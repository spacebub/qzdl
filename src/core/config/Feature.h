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

#include <array>
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
    skill,
    noMonsters,
    fast,
    warp,

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

    pwads,
    deh,
    bex,

    // Every port has -record and -playdemo. The rest varies.
    demos,
    playdemo,
    record,
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
    noAutoload,
    nosfx,

    // A port has both or neither.
    fullscreen,
    windowed,

    // The port saves the display choice, so it outlives the launch.
    displayKept,

    shotdir,

    // The netgame features follow from Port::netgame, except those a dialect decides one by one.
    hosts,
    joins,

    // ZDoom family only.
    players,

    server,
    privateServer,

    listing,
    netPort,
    deathmatch,
    altDeathmatch,

    // ZDoom family cvar.
    fragLimit,
    timeLimit,
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

    // The flag words the port reads, in the order of Flags::Word.
    dmflags,
    dmflags2,
    dmflags3,
    zadmflags,
    lmsWeapons,
    lmsSpectators,
    compatflags,
    compatflags2,
    zacompatflags,

    // The port writes its compatibility words to its own settings, where they outlive the launch.
    keepsCompatflags,

    // The port reads a flag word with strtol, which caps at 2^31 - 1 where long is 32 bits.
    signedWords,

    // Follows from Port::complevel.
    complevel,

    // On Windows the port prints to a pipe only when given -stdout.
    stdoutSwitch,

    count,
};

static_assert(static_cast<unsigned>(Feature::count) <= 128, "the features fit two 64 bit words");

struct Features {
    std::array<std::uint64_t, 2> words{};

    constexpr Features() = default;

    // NOLINTNEXTLINE(misc-explicit-constructor): a lone feature stands for a mask in the rows and in a | b.
    constexpr Features(const Feature one) {
        const auto at = static_cast<unsigned>(one);
        words[at / 64] = std::uint64_t{1} << (at % 64);
    }

    [[nodiscard]] constexpr bool has(const Feature one) const {
        const auto at = static_cast<unsigned>(one);

        return ((words[at / 64] >> (at % 64)) & 1U) != 0;
    }

    constexpr Features &operator|=(const Features other) {
        words[0] |= other.words[0];
        words[1] |= other.words[1];

        return *this;
    }

    constexpr Features &operator&=(const Features other) {
        words[0] &= other.words[0];
        words[1] &= other.words[1];

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

[[nodiscard]] constexpr Features operator~(Features all) {
    all.words[0] = ~all.words[0];
    all.words[1] = ~all.words[1];

    return all;
}

}
