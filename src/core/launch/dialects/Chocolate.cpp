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

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

#include "core/launch/dialects/Dialects.h"

namespace Dialect::dialects {

namespace {

struct GameVersion {
    std::string_view word;
    std::string_view name;

    // The oldest list that holds it.
    Complevels list;
};

// Stored by place from GAME_VERSIONS on, so new ones go at the end.
constexpr std::array VERSIONS = std::to_array<GameVersion>({
        {"1.2", "Doom 1.2", Complevels::gameVersion12},
        {"1.5", "Doom 1.5", Complevels::gameVersion},
        {"1.666", "Doom 1.666", Complevels::gameVersion1666},
        {"1.7", "Doom 1.7", Complevels::gameVersion1666},
        {"1.8", "Doom 1.8", Complevels::gameVersion1666},
        {"1.9", "Doom 1.9", Complevels::gameVersion19},
        {"ultimate", "Ultimate Doom", Complevels::gameVersion19},
        {"final", "Final Doom", Complevels::gameVersion19},
        {"final2", "Final Doom, other release", Complevels::gameVersion19},
        {"hacx", "Hacx", Complevels::gameVersion19},
        {"chex", "Chex Quest", Complevels::gameVersion19},
});

[[nodiscard]] constexpr bool onList(const GameVersion &version, const std::size_t list) {
    return listPlace(version.list) <= list;
}

[[nodiscard]] constexpr std::size_t countOn(const std::size_t list) {
    return static_cast<std::size_t>(
            std::ranges::count_if(VERSIONS, [list](const GameVersion &each) { return onList(each, list); }));
}

// -1 first, the port's own. A game version keeps its number on every list.
template <std::size_t LIST>
constexpr std::array<int, countOn(LIST) + 1> LEVELS = [] {
    std::array<int, countOn(LIST) + 1> out{-1};
    std::size_t filled = 1;

    for (std::size_t at = 0; at < VERSIONS.size(); ++at) {
        if (onList(VERSIONS[at], LIST)) {
            out[filled] = GAME_VERSIONS + static_cast<int>(at);
            ++filled;
        }
    }

    return out;
}();

static_assert(LEVELS<listPlace(Complevels::gameVersion)>.size() == VERSIONS.size() + 1,
              "gameVersion holds every game version");

const GameVersion *versionAt(const int level) {
    const int at = level - GAME_VERSIONS;

    return at >= 0 && std::cmp_less(at, VERSIONS.size()) ? &VERSIONS[static_cast<size_t>(at)] : nullptr;
}

std::string_view gameVersionWord(const int level) {
    const GameVersion *version = versionAt(level);

    return version == nullptr ? std::string_view() : version->word;
}

template <std::size_t... AT>
[[nodiscard]] constexpr std::array<Numbering, sizeof...(AT)>
numberings([[maybe_unused]] std::index_sequence<AT...> lists) {
    return {Numbering{.levels = LEVELS<AT>, .levelWord = gameVersionWord}...};
}

template <Complevels LIST>
constexpr void readsUpTo(Traits &traits) {
    static_assert(listsGameVersions(LIST), "a row picks one of the game version lists");

    traits.complevel = LIST;
}

constexpr std::array CRISPY_CHANGES =
        join(CHOCOLATE_LINE_CHANGES, std::to_array<Spelling>({{complevel, {"-gameversion"}}}));

// Chocolate Doom finds the sprites and flats of a PWAD only when it is merged in.
constexpr std::array CHOCOLATE_CHANGES = join(CRISPY_CHANGES, std::to_array<Spelling>({{pwads, {"-merge"}}}));

// From 1.5 it merges every -file on its own.
constexpr Traits crispy(Traits traits) {
    traits.features |= levelstat | pistolStart | coopSpawns | doubleAmmo | flipLevels | flipWeapons | deathmatch3;
    traits.configName = "crispy";
    traits.changes = CRISPY_CHANGES;
    traits.line = PortLine::crispy;

    return defined(traits);
}

// Heretic, Hexen and Strife come in the same packages, with game versions and switches of their own.
constexpr Traits otherGame(Traits traits) {
    traits.complevel = Complevels::none;
    traits.features &= ~(deathmatch3 | freedoomFolders);
    traits.line = PortLine::none;

    // Of the rules, they read only pistol start.
    traits.features &= ~Options::RULES | pistolStart;

    return defined(traits);
}

// -gameversion 1.5 is read from 3.1.1.
constexpr Era CHOCOLATE_3_1_0{.special = readsUpTo<Complevels::gameVersion12>};

// -noautoload, -gameversion 1.2 and a separate save folder for each of Freedoom and FreeDM come in 3.1.0.
constexpr Era CHOCOLATE_3_0{.lacks = noAutoload | freedoomFolders, .special = readsUpTo<Complevels::gameVersion1666>};

// -savedir is read from 3.0.0. Before it the settings and saves live in ~/.chocolate-doom off Windows.
constexpr Era CHOCOLATE_2_1{
        .lacks = CHOCOLATE_3_0.lacks | saveFolder,
        .gains = homeDotFolder,
        .special = readsUpTo<Complevels::gameVersion1666>,
};

// -gameversion 1.666, 1.7 and 1.8 are read from 2.1.0.
constexpr Era CHOCOLATE_2_0{
        .lacks = CHOCOLATE_2_1.lacks,
        .gains = homeDotFolder,
        .special = readsUpTo<Complevels::gameVersion19>,
};

// -gameversion 1.5 is read from 7.1.
constexpr Era CRISPY_7_0{.special = readsUpTo<Complevels::gameVersion12>};

// -coop_spawns is read from 7.0.
constexpr Era CRISPY_5_11{.lacks = coopSpawns, .special = readsUpTo<Complevels::gameVersion12>};

// -doubleammo is read from 5.11.
constexpr Era CRISPY_5_9_2{.lacks = CRISPY_5_11.lacks | doubleAmmo, .special = readsUpTo<Complevels::gameVersion12>};

// -pistolstart is read from 5.9.2.
constexpr Era CRISPY_5_9_0{.lacks = CRISPY_5_9_2.lacks | pistolStart, .special = readsUpTo<Complevels::gameVersion12>};

// -levelstat and a separate save folder for each of Freedoom and FreeDM come in 5.9.0.
constexpr Era CRISPY_5_6_4{
        .lacks = CRISPY_5_9_0.lacks | levelstat | freedoomFolders,
        .special = readsUpTo<Complevels::gameVersion12>,
};

// -gameversion 1.2 is read from 5.6.4.
constexpr Era CRISPY_5_4{.lacks = CRISPY_5_6_4.lacks, .special = readsUpTo<Complevels::gameVersion1666>};

// -noautoload is read from 5.4.
constexpr Era CRISPY_5_3{.lacks = CRISPY_5_4.lacks | noAutoload, .special = readsUpTo<Complevels::gameVersion1666>};

// -flipweapons is read from 5.3.
constexpr Era CRISPY_5_0{.lacks = CRISPY_5_3.lacks | flipWeapons, .special = readsUpTo<Complevels::gameVersion1666>};

// Before 5.0 the settings and saves live in ~/.crispy-doom off Windows.
constexpr Era CRISPY_4_1{
        .lacks = CRISPY_5_0.lacks,
        .gains = homeDotFolder,
        .special = readsUpTo<Complevels::gameVersion1666>,
};

// -dm3 is read from 4.1.
constexpr Era CRISPY_4_0{
        .lacks = CRISPY_4_1.lacks | deathmatch3,
        .gains = homeDotFolder,
        .special = readsUpTo<Complevels::gameVersion1666>,
};

// -savedir is read from 4.0.
constexpr Era CRISPY_1_5{
        .lacks = CRISPY_4_0.lacks | saveFolder,
        .gains = homeDotFolder,
        .special = readsUpTo<Complevels::gameVersion1666>,
};

// Every -file is merged in from 1.5.
constexpr Era CRISPY_1_3{
        .lacks = CRISPY_1_5.lacks,
        .gains = homeDotFolder,
        .changes = CHOCOLATE_CHANGES,
        .special = readsUpTo<Complevels::gameVersion1666>,
};

// -fliplevels is read from 1.3.
constexpr Era CRISPY_1_1{
        .lacks = CRISPY_1_3.lacks | flipLevels,
        .gains = homeDotFolder,
        .changes = CHOCOLATE_CHANGES,
        .special = readsUpTo<Complevels::gameVersion1666>,
};

// -gameversion 1.666, 1.7 and 1.8 are read from 1.1.
constexpr Era CRISPY_1_0{
        .lacks = CRISPY_1_1.lacks,
        .gains = homeDotFolder,
        .changes = CHOCOLATE_CHANGES,
        .special = readsUpTo<Complevels::gameVersion19>,
};

}

constexpr Traits CHOCOLATE_TRAITS = defined({
        .features = COMMON | recognised | iwad | extraConfig | configFile | saveFolder | deh | bex | noAutoload | nosfx
                    | fullscreen | windowed | respawn | loadgame | replays | timedemo | longtics | soloNet
                    | recordAddsLmp | nomusic | displayKept | noCheats | turbo | freedoomFolders,
        .saveExt = ".dsg",
        .configName = "chocolate",
        .changes = CHOCOLATE_CHANGES,
        .dialect = Dialect::chocolate,
        .loads = SaveNames::slot,
        .netgame = Netgames::chocolate,
        .complevel = Complevels::gameVersion,
        .line = PortLine::chocolate,
});

constexpr Traits CRISPY_TRAITS = crispy(CHOCOLATE_TRAITS);
constexpr Traits CRISPY_OTHER_TRAITS = otherGame(CRISPY_TRAITS);
constexpr Traits CHOCOLATE_OTHER_TRAITS = otherGame(CHOCOLATE_TRAITS);

// Each row starts at the first release it describes. An older release runs as the oldest row.
constexpr std::array CHOCOLATE_RANGES = std::to_array<VersionRange>({
        row("3.1.1", "3.1.1 and later", NEWEST),
        row("3.1.0", "3.1.0", CHOCOLATE_3_1_0),
        row("3.0.0", "3.0.0 to 3.0.1", CHOCOLATE_3_0),
        row("2.1.0", "2.1.0 to 2.3.0", CHOCOLATE_2_1),
        row("2.0.0", "2.0.0", CHOCOLATE_2_0),
});

constexpr Releases<CHOCOLATE_RANGES.size()> CHOCOLATE_RELEASES(CHOCOLATE_TRAITS, CHOCOLATE_RANGES);

constexpr std::array CRISPY_RANGES = std::to_array<VersionRange>({
        row("7.1", "7.1 and later", NEWEST),
        row("7.0", "7.0", CRISPY_7_0),
        row("5.11", "5.11 to 6.0", CRISPY_5_11),
        row("5.9.2", "5.9.2 to 5.10.3", CRISPY_5_9_2),
        row("5.9.0", "5.9.0 to 5.9.1", CRISPY_5_9_0),
        row("5.6.4", "5.6.4 to 5.8.0", CRISPY_5_6_4),
        row("5.4", "5.4 to 5.6.3", CRISPY_5_4),
        row("5.3", "5.3", CRISPY_5_3),
        row("5.0", "5.0 to 5.2", CRISPY_5_0),
        row("4.1", "4.1 to 4.3", CRISPY_4_1),
        row("4.0", "4.0", CRISPY_4_0),
        row("1.5", "1.5 to 3.5", CRISPY_1_5),
        row("1.3", "1.3 to 1.4", CRISPY_1_3),
        row("1.1", "1.1 to 1.2", CRISPY_1_1),
        row("1.0", "1.0", CRISPY_1_0),
});

constexpr Releases<CRISPY_RANGES.size()> CRISPY_RELEASES(CRISPY_TRAITS, CRISPY_RANGES);

std::string_view gameVersionName(const int level) {
    const GameVersion *version = versionAt(level);

    return version == nullptr ? std::string_view() : version->name;
}

constexpr std::array<Numbering, GAME_VERSION_LISTS> GAME_VERSION_NUMBERINGS =
        numberings(std::make_index_sequence<GAME_VERSION_LISTS>());

}
