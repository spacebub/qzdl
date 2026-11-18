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

#include "core/launch/dialects/Dialects.h"

namespace Dialect::dialects {

namespace {

// -1 first, the port's own.
constexpr std::array PRBOOM_LEVELS = {-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, MBF, 12, 13, 14, 15, 16, 17};
constexpr std::array DSDA_LEVELS = {-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, MBF, 12, 13, 14, 15, 16, 17, MBF21};

constexpr std::array PRBOOM_OWN = std::to_array<Spelling>({
        {saveFolder, {"-save"}},
        {noAutoload, {"-noload"}},
        {fullscreen, {"-nowindow"}},
        {joins, {"-net"}},
});

constexpr std::array PRBOOM_CHANGES = join(TURBO_TO_400, PRBOOM_OWN);

// NOLINTBEGIN(readability-trailing-comma): a change is one row, with its description inside it
constexpr std::array DSDA_CHANGES = std::to_array<Spelling>({
        {saveFolder, {"-save"}},
        {gameSpeed,
         {"-game_speed",
          "Game speed in percent, from 10 to 10000. Strict mode keeps it at 100.",
          {},
          Range{.least = 10, .most = 10000}}},
});
// NOLINTEND(readability-trailing-comma)

constexpr std::array ETERNITY_OWN = std::to_array<Spelling>({
        {saveFolder, {"-save"}},
        {noAutoload, {"-noload"}},
        {bex, {"-bex"}},
        {dmflags, {"-dmflags"}},
});

constexpr std::array ETERNITY_CHANGES = join(TURBO_TO_400, ETERNITY_OWN);

// -loadgame is read from 0.30.0.
constexpr Era DSDA_0_28_3{.lacks = loadgame};

// -save wins over organized saves from 0.28.3.
constexpr Era DSDA_0_28{.lacks = DSDA_0_28_3.lacks, .gains = organizedSavesWin};

// From 0.28.0 -recordfromto needs no -skill. The dot folder in the home is used only when it is there.
constexpr Era DSDA_0_27{.lacks = DSDA_0_28.lacks, .gains = DSDA_0_28.gains | continuesWithSkill | homeDotFolder};

// -record stops without -skill from 0.27.0.
constexpr Era DSDA_0_26{.lacks = DSDA_0_27.lacks | recordsWithSkill, .gains = DSDA_0_28.gains | homeDotFolder};

// -chain_episodes and -game_speed are read from 0.26.0. The data root stops taking a folder slot there.
constexpr Era DSDA_0_25{
        .lacks = DSDA_0_26.lacks | chainEpisodes | gameSpeed,
        .gains = DSDA_0_26.gains | dataRootTakesSlot,
};

// -record stops without -complevel from 0.25.0.
constexpr Era DSDA_0_22_3{.lacks = DSDA_0_25.lacks | recordsWithComplevel, .gains = DSDA_0_25.gains};

// -loadgame is ignored from 0.22.3 on.
constexpr Era DSDA_0_21_2{.lacks = DSDA_0_22_3.lacks & ~Features(loadgame), .gains = DSDA_0_25.gains};

// -pistolstart is read from 0.21.2.
constexpr Era DSDA_0_21_1{.lacks = DSDA_0_21_2.lacks | pistolStart, .gains = DSDA_0_25.gains};

// -recordfromto stops without -skill from Nyan Doom 1.1.5 to 1.1.7.
constexpr Era NYAN_1_1_1{.lacks = DSDA_0_28.lacks, .gains = DSDA_0_26.gains};

// -lr is read from Nyan Doom 1.1.1.
constexpr Era NYAN_1_1{.lacks = NYAN_1_1_1.lacks | limitRemoving, .gains = NYAN_1_1_1.gains};

}

constexpr Traits PRBOOM_TRAITS = defined({
        .features = COMMON | recognised | iwad | configFile | saveFolder | deh | bex | noAutoload | nosfx | fullscreen
                    | windowed | respawn | longWarp | loadgame | replays | timedemo | fastdemo | longtics | soloNet
                    | recordAddsLmp | continues | levelstat | pistolStart | nomusic | shotdir | coopSpawns | noCheats
                    | stroller | turbo | dogs,
        .saveExt = ".dsg",
        .configName = "prboom",
        .changes = PRBOOM_CHANGES,
        .dialect = Dialect::prboom,
        .loads = SaveNames::slot,
        .netgame = Netgames::prboom,
        .complevel = Complevels::prboom,
        .line = PortLine::prboom,
});

// GLBoom and Rude keep configs of their own.
constexpr Traits GLBOOM_TRAITS = withConfig(PRBOOM_TRAITS, "glboom");
constexpr Traits RUDE_TRAITS = withConfig(PRBOOM_TRAITS, "rude");

constexpr Traits DSDA_TRAITS = defined({
        .features = COMMON | recognised | iwad | configFile | saveFolder | deh | bex | noAutoload | nosfx | fullscreen
                    | windowed | respawn | longWarp | loadgame | replays | timedemo | fastdemo | recordsWithComplevel
                    | recordsWithSkill | longtics | soloNet | recordAddsLmp | continues | continueComplevel | levelstat
                    | pistolStart | nomusic | shotdir | coopSpawns | chainEpisodes | noCheats | stroller | turbo
                    | gameSpeed,
        .saveExt = ".dsg",
        .configName = "dsda",
        .changes = DSDA_CHANGES,
        .lastSlot = 118,
        .dialect = Dialect::dsda,
        .loads = SaveNames::slot,
        .netgame = Netgames::none,
        .complevel = Complevels::dsda,
        .line = PortLine::dsda,
});

// A DSDA-Doom fork with its own config.
constexpr Traits NYAN_TRAITS = [] {
    Traits nyan = DSDA_TRAITS;
    nyan.features |= limitRemoving;
    nyan.configName = "nyan";
    nyan.line = PortLine::nyan;

    return defined(nyan);
}();

constexpr Traits ETERNITY_TRAITS = defined({
        .features = COMMON | recognised | iwad | configFile | saveFolder | deh | bex | noAutoload | nosfx | respawn
                    | longWarp | loadgame | replays | timedemo | fastdemo | soloNet | recordAddsLmp | nomusic
                    | displayKept | vanillaMode | turbo | gameSpeed | dogs | dmflags,
        .saveExt = ".dsg",
        .configName = "eternity",
        .changes = ETERNITY_CHANGES,
        .dialect = Dialect::eternity,
        .loads = SaveNames::slot,
        .netgame = Netgames::none,
        .line = PortLine::eternity,
});

// Each row starts at the first release it describes. An older release runs as the oldest row. That row is named by
// its last release, since nothing opens it from below.
constexpr std::array DSDA_RANGES = std::to_array<VersionRange>({
        row("0.30.0", "0.30.0 and later", NEWEST),
        row("0.28.3", "0.28.3 to 0.29.4", DSDA_0_28_3),
        row("0.28.0", "0.28.0 to 0.28.2", DSDA_0_28),
        row("0.27.0", "0.27.0 to 0.27.5", DSDA_0_27),
        row("0.26.0", "0.26.0 to 0.26.2", DSDA_0_26),
        row("0.25.0", "0.25.0 to 0.25.6", DSDA_0_25),
        row("0.22.3", "0.22.3 to 0.24.3", DSDA_0_22_3),
        row("0.21.2", "0.21.2 to 0.22.2", DSDA_0_21_2),
        row("0.21.1", "0.21.1 and earlier", DSDA_0_21_1),
});

constexpr Releases<DSDA_RANGES.size()> DSDA_RELEASES(DSDA_TRAITS, DSDA_RANGES);

// Nyan Doom stops on any switch it does not know, so an older release must get none of the newer ones.
constexpr std::array NYAN_RANGES = std::to_array<VersionRange>({
        row("1.6.1", "1.6.1 and later", NEWEST),
        row("1.2.1", "1.2.1 to 1.6.0", DSDA_0_28_3),
        row("1.1.8", "1.1.8 to 1.2.0", DSDA_0_28),
        row("1.1.5", "1.1.5 to 1.1.7", DSDA_0_27),
        row("1.1.1", "1.1.1 to 1.1.4", NYAN_1_1_1),
        row("1.1.0", "1.1.0", NYAN_1_1),
});

constexpr Releases<NYAN_RANGES.size()> NYAN_RELEASES(NYAN_TRAITS, NYAN_RANGES);

constexpr Numbering PRBOOM_NUMBERING{.levels = PRBOOM_LEVELS, .levelWord = nullptr};

constexpr Numbering DSDA_NUMBERING{.levels = DSDA_LEVELS, .levelWord = nullptr};

}
