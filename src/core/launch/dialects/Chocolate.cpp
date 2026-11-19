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

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

#include "core/launch/dialects/Dialects.h"

namespace Dialect::dialects {

namespace {

struct GameVersion {
    std::string_view word;
    std::string_view name;

    // Plays plutonia.wad and tnt.wad as Doom II.
    bool beforeFinal;
};

// Stored as GAME_VERSIONS plus the place, so new ones go at the end.
constexpr std::array VERSIONS = std::to_array<GameVersion>({
        {"1.2", "Doom 1.2", true},
        {"1.5", "Doom 1.5", true},
        {"1.666", "Doom 1.666", true},
        {"1.7", "Doom 1.7", true},
        {"1.8", "Doom 1.8", true},
        {"1.9", "Doom 1.9", true},
        {"ultimate", "Ultimate Doom", true},
        {"final", "Final Doom", false},
        {"final2", "Final Doom, other release", false},
        {"hacx", "Hacx", true},
        {"chex", "Chex Quest", false},
});

// The stored number of each game version, in the order of VERSIONS.
enum : std::uint8_t {
    V1_2 = GAME_VERSIONS,
    V1_5,
    V1_666,
    V1_7,
    V1_8,
    V1_9,
    ULTIMATE,
    FINAL,
    FINAL2,
    HACX,
    CHEX,
};

static_assert(CHEX - GAME_VERSIONS + 1 == VERSIONS.size(), "a number for every game version");

// The versions each list offers, -1 first for the port's own. A game version keeps its number on every list.
constexpr auto FROM_1_9 = std::to_array<int>({-1, V1_9, ULTIMATE, FINAL, FINAL2, HACX, CHEX});
constexpr auto FROM_1_666 = std::to_array<int>({-1, V1_666, V1_7, V1_8, V1_9, ULTIMATE, FINAL, FINAL2, HACX, CHEX});
constexpr auto WITHOUT_1_5 =
        std::to_array<int>({-1, V1_2, V1_666, V1_7, V1_8, V1_9, ULTIMATE, FINAL, FINAL2, HACX, CHEX});
constexpr auto EVERY_VERSION =
        std::to_array<int>({-1, V1_2, V1_5, V1_666, V1_7, V1_8, V1_9, ULTIMATE, FINAL, FINAL2, HACX, CHEX});

const GameVersion *versionAt(const int level) {
    const int at = level - GAME_VERSIONS;

    return at >= 0 && std::cmp_less(at, VERSIONS.size()) ? &VERSIONS[static_cast<size_t>(at)] : nullptr;
}

std::string_view gameVersionWord(const int level) {
    const GameVersion *version = versionAt(level);

    return version == nullptr ? std::string_view() : version->word;
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

// From 1.5 it merges every -file on its own. From 3.0 a patch without the header only prints a warning.
constexpr Traits crispy(Traits traits) {
    traits.features |= levelstat | pistolStart | coopSpawns | doubleAmmo | flipLevels | flipWeapons | deathmatch3
                       | numbersTakenDemo | pwadExpansions | recordingStopsExpansions;
    traits.features &= ~Features(strictPatchHeader);
    traits.configName = "crispy";
    traits.changes = CRISPY_CHANGES;
    traits.line = PortLine::crispy;

    return defined(traits);
}

// Heretic, Hexen and Strife come in the same packages. They offer no game versions and read fewer switches.
constexpr Traits otherGame(Traits traits, const Features unread, const std::span<const Spelling> changes = {}) {
    traits.features &=
            ~(unread | deathmatch3 | flipLevels | freedoomFolders | pwadExpansions | recordingStopsExpansions);
    traits.complevel = Complevels::none;
    traits.line = PortLine::none;

    if (!changes.empty()) {
        traits.changes = changes;
    }

    return defined(traits);
}

constexpr Features HERETIC_UNREAD = turbo | altDeathmatch | noCheats | doubleAmmo | flipWeapons;

// -loadgame reads a single digit.
constexpr int HERETIC_LAST_SLOT = 9;

constexpr Traits heretic(Traits traits, const Features unread, const std::span<const Spelling> changes = {}) {
    traits.saveExt = ".hsg";
    traits.patchHeader = PatchHeader::hhe;
    traits.lastSlot = HERETIC_LAST_SLOT;

    return otherGame(traits, unread, changes);
}

constexpr Features HEXEN_UNREAD = HERETIC_UNREAD | deh | bex | strictPatchHeader | pistolStart | levelstat | coopSpawns;
constexpr Features STRIFE_UNREAD = pistolStart | levelstat | coopSpawns;

constexpr Traits hexen(Traits traits, const Features unread, const std::span<const Spelling> changes = {}) {
    traits.saveExt = ".hxs";
    traits.saveLayout = SaveLayout::hub;
    traits.warpNumber = WarpNumber::warpTrans;

    return otherGame(traits, unread, changes);
}

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

// The menu offers slots 0 to 5. Slot 6 holds the game in play.
constexpr int STRIFE_LAST_SLOT = 5;

constexpr Traits strife(Traits traits, const std::span<const Spelling> changes) {
    traits.features |= alwaysDeathmatch;
    traits.saveExt = ".ssg";
    traits.lastSlot = STRIFE_LAST_SLOT;
    traits.patchHeader = PatchHeader::sehacked;
    traits.saveLayout = SaveLayout::folder;

    return otherGame(traits, STRIFE_UNREAD, changes);
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

// Before 3.0 a patch without the header stops the port, as in Chocolate Doom.
constexpr Era CRISPY_STRICT_PATCHES{.gains = strictPatchHeader};

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
constexpr Era CRISPY_HERETIC_5_10 = older(CRISPY_HERETIC_5_11, CRISPY_RAVEN_BEFORE_FAST);

// From 5.10.0 a demo already there gets a numbered name beside it.
constexpr Era CRISPY_HERETIC_5_9_2 = older(CRISPY_HERETIC_5_10, {.lacks = numbersTakenDemo});

// -wandstart is read from 5.9.2.
constexpr Era CRISPY_HERETIC_5_9_0 = older(CRISPY_HERETIC_5_9_2, {.lacks = pistolStart});

// -levelstat is read from 5.9.0.
constexpr Era CRISPY_HERETIC_5_4 = older(CRISPY_HERETIC_5_9_0, {.lacks = levelstat});
constexpr Era CRISPY_HERETIC_5_0 = older(CRISPY_HERETIC_5_4, CRISPY_BEFORE_NOAUTOLOAD);
constexpr Era CRISPY_HERETIC_4_2 = older(CRISPY_HERETIC_5_0, CRISPY_IN_DOT_FOLDER);
constexpr Era CRISPY_HERETIC_3_0 = older(CRISPY_HERETIC_4_2, CRISPY_RAVEN_BEFORE_SAVEDIR);
constexpr Era CRISPY_HERETIC_1_5 = older(CRISPY_HERETIC_3_0, CRISPY_STRICT_PATCHES);
constexpr Era CRISPY_HERETIC_1_0 = older(CRISPY_HERETIC_1_5, CRISPY_BEFORE_MERGE);

// Hexen and Strife give a demo already there a numbered name beside it from 7.2.
constexpr Era CRISPY_RAVEN_BEFORE_NUMBERING{.lacks = numbersTakenDemo};

constexpr Era CRISPY_HEXEN_5_4 = older(CRISPY_RAVEN_BEFORE_NUMBERING, CRISPY_RAVEN_BEFORE_FAST);
constexpr Era CRISPY_HEXEN_5_0 = older(CRISPY_HEXEN_5_4, CRISPY_BEFORE_NOAUTOLOAD);
constexpr Era CRISPY_HEXEN_4_2 = older(CRISPY_HEXEN_5_0, CRISPY_IN_DOT_FOLDER);
constexpr Era CRISPY_HEXEN_1_5 = older(CRISPY_HEXEN_4_2, CRISPY_RAVEN_BEFORE_SAVEDIR);
constexpr Era CRISPY_HEXEN_1_0 = older(CRISPY_HEXEN_1_5, CRISPY_BEFORE_MERGE);

// -flipweapons is read from 7.0.
constexpr Era CRISPY_STRIFE_5_11 = older(CRISPY_RAVEN_BEFORE_NUMBERING, {.lacks = flipWeapons});

// -doubleammo is read from 5.11.
constexpr Era CRISPY_STRIFE_5_4 = older(CRISPY_STRIFE_5_11, {.lacks = doubleAmmo});
constexpr Era CRISPY_STRIFE_5_0 = older(CRISPY_STRIFE_5_4, CRISPY_BEFORE_NOAUTOLOAD);
constexpr Era CRISPY_STRIFE_4_2 = older(CRISPY_STRIFE_5_0, CRISPY_IN_DOT_FOLDER);
constexpr Era CRISPY_STRIFE_3_0 = older(CRISPY_STRIFE_4_2, CRISPY_BEFORE_SAVEDIR);
constexpr Era CRISPY_STRIFE_1_5 = older(CRISPY_STRIFE_3_0, CRISPY_STRICT_PATCHES);
constexpr Era CRISPY_STRIFE_1_0 = older(CRISPY_STRIFE_1_5, {.changes = CHOCOLATE_STRIFE_CHANGES});

// -gameversion 1.5 is read from 3.1.1.
constexpr Era CHOCOLATE_3_1_0{.special = readsUpTo<Complevels::gameVersion12>};

// -noautoload, -gameversion 1.2 and a separate save folder for each of Freedoom and FreeDM come in 3.1.0.
constexpr Era CHOCOLATE_3_0{.lacks = noAutoload | freedoomFolders, .special = readsUpTo<Complevels::gameVersion1666>};

constexpr Era CHOCOLATE_2_1 = older(CHOCOLATE_3_0, CHOCOLATE_BEFORE_SAVEDIR);

// -gameversion 1.666, 1.7 and 1.8 are read from 2.1.0.
constexpr Era CHOCOLATE_2_0 = older(CHOCOLATE_2_1, {.special = readsUpTo<Complevels::gameVersion19>});

// -gameversion 1.5 is read from 7.1, and from 7.1 -record stops the PWAD expansions.
constexpr Era CRISPY_7_0{.lacks = recordingStopsExpansions, .special = readsUpTo<Complevels::gameVersion12>};

// -coop_spawns is read from 7.0.
constexpr Era CRISPY_5_11 = older(CRISPY_7_0, {.lacks = coopSpawns});

// -doubleammo is read from 5.11. Releases before 5.11 also play the PWAD expansions as missions. They are not
// modelled.
constexpr Era CRISPY_5_9_2 = older(CRISPY_5_11, {.lacks = doubleAmmo | pwadExpansions});

// -pistolstart is read from 5.9.2.
constexpr Era CRISPY_5_9_0 = older(CRISPY_5_9_2, {.lacks = pistolStart});

// -levelstat and a separate save folder for each of Freedoom and FreeDM come in 5.9.0.
constexpr Era CRISPY_5_6_4 = older(CRISPY_5_9_0, {.lacks = levelstat | freedoomFolders});

// -gameversion 1.2 is read from 5.6.4.
constexpr Era CRISPY_5_4 = older(CRISPY_5_6_4, {.special = readsUpTo<Complevels::gameVersion1666>});
constexpr Era CRISPY_5_3 = older(CRISPY_5_4, CRISPY_BEFORE_NOAUTOLOAD);

// -flipweapons is read from 5.3.
constexpr Era CRISPY_5_0 = older(CRISPY_5_3, {.lacks = flipWeapons});
constexpr Era CRISPY_4_1 = older(CRISPY_5_0, CRISPY_IN_DOT_FOLDER);

// -dm3 is read from 4.1.
constexpr Era CRISPY_4_0 = older(CRISPY_4_1, {.lacks = deathmatch3});
constexpr Era CRISPY_3_0 = older(CRISPY_4_0, CRISPY_BEFORE_SAVEDIR);
constexpr Era CRISPY_1_5 = older(CRISPY_3_0, CRISPY_STRICT_PATCHES);
constexpr Era CRISPY_1_3 = older(CRISPY_1_5, CRISPY_BEFORE_MERGE);

// -fliplevels is read from 1.3, and from 1.3 a demo already there gets a numbered name beside it.
constexpr Era CRISPY_1_1 = older(CRISPY_1_3, {.lacks = flipLevels | numbersTakenDemo});

// -gameversion 1.666, 1.7 and 1.8 are read from 1.1.
constexpr Era CRISPY_1_0 = older(CRISPY_1_1, {.special = readsUpTo<Complevels::gameVersion19>});

}

constexpr Traits CHOCOLATE_TRAITS = defined({
        .features = COMMON | recognised | iwad | extraConfig | configFile | saveFolder | deh | bex | strictPatchHeader
                    | noAutoload | nosfx | fullscreen | windowed | respawn | loadgame | replays | timedemo | longtics
                    | soloNet | recordsBareName | nomusic | displayKept | noCheats | turbo | freedoomFolders
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
constexpr Traits CHOCOLATE_HERETIC_TRAITS = heretic(CHOCOLATE_TRAITS, HERETIC_UNREAD | fast);
constexpr Traits CHOCOLATE_HEXEN_TRAITS = hexen(CHOCOLATE_TRAITS, HEXEN_UNREAD | fast);
constexpr Traits CHOCOLATE_STRIFE_TRAITS = strife(CHOCOLATE_TRAITS, CHOCOLATE_STRIFE_CHANGES);
constexpr Traits CRISPY_HERETIC_TRAITS = heretic(CRISPY_TRAITS, HERETIC_UNREAD, CRISPY_HERETIC_CHANGES);
constexpr Traits CRISPY_HEXEN_TRAITS = hexen(CRISPY_TRAITS, HEXEN_UNREAD, CRISPY_RAVEN_CHANGES);
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
        row("3.0", "3.0 to 3.5", CRISPY_3_0),
        row("1.5", "1.5 to 2.3", CRISPY_1_5),
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
        row("5.10.0", "5.10.0 to 5.10.3", CRISPY_HERETIC_5_10),
        row("5.9.2", "5.9.2", CRISPY_HERETIC_5_9_2),
        row("5.9.0", "5.9.0 to 5.9.1", CRISPY_HERETIC_5_9_0),
        row("5.4", "5.4 to 5.8.0", CRISPY_HERETIC_5_4),
        row("5.0", "5.0 to 5.3", CRISPY_HERETIC_5_0),
        row("4.2", "4.2 to 4.3", CRISPY_HERETIC_4_2),
        row("3.0", "3.0 to 3.3", CRISPY_HERETIC_3_0),
        row("1.5", "1.5 to 2.3", CRISPY_HERETIC_1_5),
        row("1.0", "1.0 to 1.4", CRISPY_HERETIC_1_0),
});

constexpr Releases<CRISPY_HERETIC_RANGES.size()> CRISPY_HERETIC_RELEASES(CRISPY_HERETIC_TRAITS, CRISPY_HERETIC_RANGES);

constexpr std::array CRISPY_HEXEN_RANGES = std::to_array<VersionRange>({
        row("7.2", "7.2 and later", NEWEST),
        row("5.11", "5.11 to 7.1", CRISPY_RAVEN_BEFORE_NUMBERING),
        row("5.4", "5.4 to 5.10.3", CRISPY_HEXEN_5_4),
        row("5.0", "5.0 to 5.3", CRISPY_HEXEN_5_0),
        row("4.2", "4.2 to 4.3", CRISPY_HEXEN_4_2),
        row("1.5", "1.5 to 3.3", CRISPY_HEXEN_1_5),
        row("1.0", "1.0 to 1.4", CRISPY_HEXEN_1_0),
});

constexpr Releases<CRISPY_HEXEN_RANGES.size()> CRISPY_HEXEN_RELEASES(CRISPY_HEXEN_TRAITS, CRISPY_HEXEN_RANGES);

constexpr std::array CRISPY_STRIFE_RANGES = std::to_array<VersionRange>({
        row("7.2", "7.2 and later", NEWEST),
        row("7.0", "7.0 to 7.1", CRISPY_RAVEN_BEFORE_NUMBERING),
        row("5.11", "5.11 to 6.0", CRISPY_STRIFE_5_11),
        row("5.4", "5.4 to 5.10.3", CRISPY_STRIFE_5_4),
        row("5.0", "5.0 to 5.3", CRISPY_STRIFE_5_0),
        row("4.2", "4.2 to 4.3", CRISPY_STRIFE_4_2),
        row("3.0", "3.0 to 3.3", CRISPY_STRIFE_3_0),
        row("1.5", "1.5 to 2.3", CRISPY_STRIFE_1_5),
        row("1.0", "1.0 to 1.4", CRISPY_STRIFE_1_0),
});

constexpr Releases<CRISPY_STRIFE_RANGES.size()> CRISPY_STRIFE_RELEASES(CRISPY_STRIFE_TRAITS, CRISPY_STRIFE_RANGES);

std::string_view gameVersionName(const int level) {
    const GameVersion *version = versionAt(level);

    return version == nullptr ? std::string_view() : version->name;
}

bool gameVersionBeforeFinal(const int level) {
    const GameVersion *version = versionAt(level);

    return version != nullptr && version->beforeFinal;
}

constexpr Numbering GAME_VERSION_19_NUMBERING{.levels = FROM_1_9, .levelWord = gameVersionWord};
constexpr Numbering GAME_VERSION_1666_NUMBERING{.levels = FROM_1_666, .levelWord = gameVersionWord};
constexpr Numbering GAME_VERSION_12_NUMBERING{.levels = WITHOUT_1_5, .levelWord = gameVersionWord};
constexpr Numbering GAME_VERSION_NUMBERING{.levels = EVERY_VERSION, .levelWord = gameVersionWord};

}
