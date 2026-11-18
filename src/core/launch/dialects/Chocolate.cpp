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
#include <span>
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

// Heretic, Hexen and Strife come in the same packages. They offer no game versions and read fewer switches.
constexpr Traits otherGame(Traits traits, const Features unread, const std::span<const Spelling> changes = {}) {
    traits.features &= ~(unread | deathmatch3 | flipLevels | freedoomFolders);
    traits.complevel = Complevels::none;
    traits.line = PortLine::none;

    if (!changes.empty()) {
        traits.changes = changes;
    }

    return defined(traits);
}

constexpr Features HERETIC_UNREAD = turbo | altDeathmatch | noCheats | doubleAmmo | flipWeapons;

// Hexen -warp takes the warptrans number MAPINFO gives a map, not the map number, so no -warp is sent.
constexpr Features HEXEN_UNREAD = HERETIC_UNREAD | deh | bex | warp | pistolStart | levelstat | coopSpawns;
constexpr Features STRIFE_UNREAD = pistolStart | levelstat | coopSpawns;

// NOLINTBEGIN(readability-trailing-comma): a change is one row, with its description inside it
constexpr std::array CRISPY_RAVEN_CHANGES =
        join(CRISPY_CHANGES,
             std::to_array<Spelling>({
                     {fast,
                      {"-fast",
                       "Monsters move and attack faster. Left out while a demo is recorded or played and in a "
                       "netgame, since the port stops on it there.",
                       {},
                       Range{},
                       RECORDING | PLAYBACK | NETGAME}},
             }));

constexpr std::array CRISPY_HERETIC_CHANGES =
        join(CRISPY_RAVEN_CHANGES,
             std::to_array<Spelling>({
                     {pistolStart,
                      {"-wandstart",
                       "Starts every map with only the staff and the wand. Left out while a demo is recorded or played "
                       "and in a netgame, since the port stops on it there.",
                       {},
                       Range{},
                       RECORDING | PLAYBACK | NETGAME}},
             }));

// Strife reads no -deathmatch. A netgame there is a deathmatch without it.
constexpr std::array STRIFE_DEATHMATCH = std::to_array<Spelling>({{deathmatch, {}}});
constexpr std::array CHOCOLATE_STRIFE_CHANGES = join(CHOCOLATE_CHANGES, STRIFE_DEATHMATCH);

constexpr std::array CRISPY_STRIFE_CHANGES =
        join(join(CRISPY_CHANGES, STRIFE_DEATHMATCH),
             std::to_array<Spelling>({
                     {flipWeapons, {"-flipweapons", "Mirrors the weapon sprites."}},
             }));
// NOLINTEND(readability-trailing-comma)

constexpr Traits strife(Traits traits, const std::span<const Spelling> changes) {
    traits.features |= alwaysDeathmatch;

    return otherGame(traits, STRIFE_UNREAD, changes);
}

// One step older than the era above. The lacks and gains add up. The changes and special of the step replace those
// above.
[[nodiscard]] constexpr Era older(const Era &above, const Era &step) {
    return {
            .lacks = above.lacks | step.lacks,
            .gains = above.gains | step.gains,
            .changes = step.changes.empty() ? above.changes : step.changes,
            .special = step.special == nullptr ? above.special : step.special,
    };
}

// -savedir is read from 3.0.0. Before it the settings and saves live in ~/.chocolate-doom off Windows.
constexpr Era CHOCOLATE_BEFORE_SAVEDIR{.lacks = saveFolder, .gains = homeDotFolder};

// -noautoload is read from 5.4.
constexpr Era CRISPY_BEFORE_NOAUTOLOAD{.lacks = noAutoload};

// Before 5.0 the settings and saves live in ~/.crispy-doom off Windows.
constexpr Era CRISPY_IN_DOT_FOLDER{.gains = homeDotFolder};

// -savedir is read from 4.0.
constexpr Era CRISPY_BEFORE_SAVEDIR{.lacks = saveFolder};

// Every -file is merged in from 1.5.
constexpr Era CRISPY_BEFORE_MERGE{.changes = CHOCOLATE_CHANGES};

// Heretic and Hexen read -fast from 5.11.
constexpr Era CRISPY_RAVEN_BEFORE_FAST{.lacks = fast};

// Heretic and Hexen read -savedir from 4.0 and -longtics from 3.5.
constexpr Era CRISPY_RAVEN_BEFORE_SAVEDIR{.lacks = CRISPY_BEFORE_SAVEDIR.lacks | longtics};

// -noautoload is read from 3.1.0.
constexpr Era CHOCOLATE_OTHER_3_0{.lacks = noAutoload};
constexpr Era CHOCOLATE_OTHER_2_3 = older(CHOCOLATE_OTHER_3_0, CHOCOLATE_BEFORE_SAVEDIR);

// Heretic and Hexen read -longtics from 2.3.0.
constexpr Era CHOCOLATE_RAVEN_2_0 = older(CHOCOLATE_OTHER_2_3, {.lacks = longtics});

// -coop_spawns is read from 7.0.
constexpr Era CRISPY_HERETIC_5_11{.lacks = coopSpawns};
constexpr Era CRISPY_HERETIC_5_9_2 = older(CRISPY_HERETIC_5_11, CRISPY_RAVEN_BEFORE_FAST);

// -wandstart is read from 5.9.2.
constexpr Era CRISPY_HERETIC_5_9_0 = older(CRISPY_HERETIC_5_9_2, {.lacks = pistolStart});

// -levelstat is read from 5.9.0.
constexpr Era CRISPY_HERETIC_5_4 = older(CRISPY_HERETIC_5_9_0, {.lacks = levelstat});
constexpr Era CRISPY_HERETIC_5_0 = older(CRISPY_HERETIC_5_4, CRISPY_BEFORE_NOAUTOLOAD);
constexpr Era CRISPY_HERETIC_4_2 = older(CRISPY_HERETIC_5_0, CRISPY_IN_DOT_FOLDER);
constexpr Era CRISPY_HERETIC_1_5 = older(CRISPY_HERETIC_4_2, CRISPY_RAVEN_BEFORE_SAVEDIR);
constexpr Era CRISPY_HERETIC_1_0 = older(CRISPY_HERETIC_1_5, CRISPY_BEFORE_MERGE);

constexpr Era CRISPY_HEXEN_5_4 = older(NEWEST, CRISPY_RAVEN_BEFORE_FAST);
constexpr Era CRISPY_HEXEN_5_0 = older(CRISPY_HEXEN_5_4, CRISPY_BEFORE_NOAUTOLOAD);
constexpr Era CRISPY_HEXEN_4_2 = older(CRISPY_HEXEN_5_0, CRISPY_IN_DOT_FOLDER);
constexpr Era CRISPY_HEXEN_1_5 = older(CRISPY_HEXEN_4_2, CRISPY_RAVEN_BEFORE_SAVEDIR);
constexpr Era CRISPY_HEXEN_1_0 = older(CRISPY_HEXEN_1_5, CRISPY_BEFORE_MERGE);

// -flipweapons is read from 7.0.
constexpr Era CRISPY_STRIFE_5_11 = older(NEWEST, {.lacks = flipWeapons});

// -doubleammo is read from 5.11.
constexpr Era CRISPY_STRIFE_5_4 = older(CRISPY_STRIFE_5_11, {.lacks = doubleAmmo});
constexpr Era CRISPY_STRIFE_5_0 = older(CRISPY_STRIFE_5_4, CRISPY_BEFORE_NOAUTOLOAD);
constexpr Era CRISPY_STRIFE_4_2 = older(CRISPY_STRIFE_5_0, CRISPY_IN_DOT_FOLDER);
constexpr Era CRISPY_STRIFE_1_5 = older(CRISPY_STRIFE_4_2, CRISPY_BEFORE_SAVEDIR);
constexpr Era CRISPY_STRIFE_1_0 = older(CRISPY_STRIFE_1_5, {.changes = CHOCOLATE_STRIFE_CHANGES});

// -gameversion 1.5 is read from 3.1.1.
constexpr Era CHOCOLATE_3_1_0{.special = readsUpTo<Complevels::gameVersion12>};

// -noautoload, -gameversion 1.2 and a separate save folder for each of Freedoom and FreeDM come in 3.1.0.
constexpr Era CHOCOLATE_3_0{.lacks = noAutoload | freedoomFolders, .special = readsUpTo<Complevels::gameVersion1666>};

constexpr Era CHOCOLATE_2_1 = older(CHOCOLATE_3_0, CHOCOLATE_BEFORE_SAVEDIR);

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

constexpr Era CRISPY_5_3 = older(CRISPY_5_4, CRISPY_BEFORE_NOAUTOLOAD);

// -flipweapons is read from 5.3.
constexpr Era CRISPY_5_0{.lacks = CRISPY_5_3.lacks | flipWeapons, .special = readsUpTo<Complevels::gameVersion1666>};

constexpr Era CRISPY_4_1 = older(CRISPY_5_0, CRISPY_IN_DOT_FOLDER);

// -dm3 is read from 4.1.
constexpr Era CRISPY_4_0{
        .lacks = CRISPY_4_1.lacks | deathmatch3,
        .gains = homeDotFolder,
        .special = readsUpTo<Complevels::gameVersion1666>,
};

constexpr Era CRISPY_1_5 = older(CRISPY_4_0, CRISPY_BEFORE_SAVEDIR);
constexpr Era CRISPY_1_3 = older(CRISPY_1_5, CRISPY_BEFORE_MERGE);

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
                    | recordAddsLmp | nomusic | displayKept | noCheats | turbo | freedoomFolders
                    | NETGAME_CHOCOLATE_OWN,
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

// Chocolate Heretic and Hexen read no -fast.
constexpr Traits CHOCOLATE_HERETIC_TRAITS = otherGame(CHOCOLATE_TRAITS, HERETIC_UNREAD | fast);
constexpr Traits CHOCOLATE_HEXEN_TRAITS = otherGame(CHOCOLATE_TRAITS, HEXEN_UNREAD | fast);
constexpr Traits CHOCOLATE_STRIFE_TRAITS = strife(CHOCOLATE_TRAITS, CHOCOLATE_STRIFE_CHANGES);
constexpr Traits CRISPY_HERETIC_TRAITS = otherGame(CRISPY_TRAITS, HERETIC_UNREAD, CRISPY_HERETIC_CHANGES);
constexpr Traits CRISPY_HEXEN_TRAITS = otherGame(CRISPY_TRAITS, HEXEN_UNREAD, CRISPY_RAVEN_CHANGES);
constexpr Traits CRISPY_STRIFE_TRAITS = strife(CRISPY_TRAITS, CRISPY_STRIFE_CHANGES);

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

// Heretic and Hexen break at the same releases.
constexpr std::array CHOCOLATE_RAVEN_RANGES = std::to_array<VersionRange>({
        row("3.1.0", "3.1.0 and later", NEWEST),
        row("3.0.0", "3.0.0 to 3.0.1", CHOCOLATE_OTHER_3_0),
        row("2.3.0", "2.3.0", CHOCOLATE_OTHER_2_3),
        row("2.0.0", "2.0.0 to 2.2.1", CHOCOLATE_RAVEN_2_0),
});

constexpr Releases<CHOCOLATE_RAVEN_RANGES.size()> CHOCOLATE_HERETIC_RELEASES(CHOCOLATE_HERETIC_TRAITS,
                                                                             CHOCOLATE_RAVEN_RANGES);
constexpr Releases<CHOCOLATE_RAVEN_RANGES.size()> CHOCOLATE_HEXEN_RELEASES(CHOCOLATE_HEXEN_TRAITS,
                                                                           CHOCOLATE_RAVEN_RANGES);

constexpr std::array CHOCOLATE_STRIFE_RANGES = std::to_array<VersionRange>({
        row("3.1.0", "3.1.0 and later", NEWEST),
        row("3.0.0", "3.0.0 to 3.0.1", CHOCOLATE_OTHER_3_0),
        row("2.0.0", "2.0.0 to 2.3.0", CHOCOLATE_OTHER_2_3),
});

constexpr Releases<CHOCOLATE_STRIFE_RANGES.size()> CHOCOLATE_STRIFE_RELEASES(CHOCOLATE_STRIFE_TRAITS,
                                                                             CHOCOLATE_STRIFE_RANGES);

// No release from 3.4 to 4.1 builds the other games, so their tables have no row between 3.3 and 4.2.
constexpr std::array CRISPY_HERETIC_RANGES = std::to_array<VersionRange>({
        row("7.0", "7.0 and later", NEWEST),
        row("5.11", "5.11 to 6.0", CRISPY_HERETIC_5_11),
        row("5.9.2", "5.9.2 to 5.10.3", CRISPY_HERETIC_5_9_2),
        row("5.9.0", "5.9.0 to 5.9.1", CRISPY_HERETIC_5_9_0),
        row("5.4", "5.4 to 5.8.0", CRISPY_HERETIC_5_4),
        row("5.0", "5.0 to 5.3", CRISPY_HERETIC_5_0),
        row("4.2", "4.2 to 4.3", CRISPY_HERETIC_4_2),
        row("1.5", "1.5 to 3.3", CRISPY_HERETIC_1_5),
        row("1.0", "1.0 to 1.4", CRISPY_HERETIC_1_0),
});

constexpr Releases<CRISPY_HERETIC_RANGES.size()> CRISPY_HERETIC_RELEASES(CRISPY_HERETIC_TRAITS, CRISPY_HERETIC_RANGES);

constexpr std::array CRISPY_HEXEN_RANGES = std::to_array<VersionRange>({
        row("5.11", "5.11 and later", NEWEST),
        row("5.4", "5.4 to 5.10.3", CRISPY_HEXEN_5_4),
        row("5.0", "5.0 to 5.3", CRISPY_HEXEN_5_0),
        row("4.2", "4.2 to 4.3", CRISPY_HEXEN_4_2),
        row("1.5", "1.5 to 3.3", CRISPY_HEXEN_1_5),
        row("1.0", "1.0 to 1.4", CRISPY_HEXEN_1_0),
});

constexpr Releases<CRISPY_HEXEN_RANGES.size()> CRISPY_HEXEN_RELEASES(CRISPY_HEXEN_TRAITS, CRISPY_HEXEN_RANGES);

constexpr std::array CRISPY_STRIFE_RANGES = std::to_array<VersionRange>({
        row("7.0", "7.0 and later", NEWEST),
        row("5.11", "5.11 to 6.0", CRISPY_STRIFE_5_11),
        row("5.4", "5.4 to 5.10.3", CRISPY_STRIFE_5_4),
        row("5.0", "5.0 to 5.3", CRISPY_STRIFE_5_0),
        row("4.2", "4.2 to 4.3", CRISPY_STRIFE_4_2),
        row("1.5", "1.5 to 3.3", CRISPY_STRIFE_1_5),
        row("1.0", "1.0 to 1.4", CRISPY_STRIFE_1_0),
});

constexpr Releases<CRISPY_STRIFE_RANGES.size()> CRISPY_STRIFE_RELEASES(CRISPY_STRIFE_TRAITS, CRISPY_STRIFE_RANGES);

std::string_view gameVersionName(const int level) {
    const GameVersion *version = versionAt(level);

    return version == nullptr ? std::string_view() : version->name;
}

constexpr std::array<Numbering, GAME_VERSION_LISTS> GAME_VERSION_NUMBERINGS =
        numberings(std::make_index_sequence<GAME_VERSION_LISTS>());

}
