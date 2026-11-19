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

#include "core/launch/dialects/Dialects.h"

namespace Dialect::dialects {

namespace {

constexpr Features ZDOOM_FAMILY = COMMON | recognised | iwad | configFile | saveFolder | deh | bex | noAutoload | nosfx
                                  | fullscreen | windowed | exec | map | respawn | loadgame | replays | timedemo
                                  | cheats | nomusic | displayKept | shotdir | alwaysApplyDmflags | turbo | dmflags
                                  | dmflags2 | compatflags | compatflags2 | colorSet | saveDirSetting;

constexpr std::array GZDOOM_CHANGES = std::to_array<Spelling>({
        // The GZDoom line takes pistol start as a cvar, which no demo or netgame refuses.
        {pistolStart, {"+sv_pistolstart", "Starts every map with only the pistol.", "1"}},
        {fullscreen, {"+vid_fullscreen", {}, "1"}},
        {windowed, {"+vid_fullscreen", {}, "0"}},
});

constexpr std::array ZDOOM_28_CHANGES = std::to_array<Spelling>({
        {fullscreen, {"+fullscreen", {}, "1"}},
        {windowed, {"+fullscreen", {}, "0"}},
});

// Where the GZDoom line keeps saves and settings.
constexpr Features GZDOOM_SAVES =
        folderPerGame | savegamesSubfolder | windowsSaveSubfolder | portableIni | portableNeedsIni;

// -loadgame takes the path it is given.
constexpr void loadsByPath(Traits &traits) {
    traits.loads = SaveNames::path;
}

// Zandronum and Skulltag clients connect to a server with -connect. Their -join is commented out.
constexpr Spelling CONNECT{joins, {"-connect"}};

// -host for a port whose MAXPLAYERS is above the eight of GZDoom.
constexpr Spelling hostUpTo(const int most) {
    return {hostPlayers, {"-host", {}, {}, Range{.least = 1, .most = most}}};
}

constexpr std::array UZDOOM_CHANGES = join(GZDOOM_CHANGES, std::to_array<Spelling>({hostUpTo(64)}));

// -host sets sv_maxclients, which stops at MAXPLAYERS.
constexpr std::array ZANDRONUM_CHANGES = join(ZDOOM_28_CHANGES, std::to_array<Spelling>({CONNECT, hostUpTo(64)}));
constexpr std::array SKULLTAG_CHANGES = join(ZDOOM_28_CHANGES, std::to_array<Spelling>({CONNECT, hostUpTo(32)}));

// From 2.8.0 dmflags bits 18 and 19 choose the mouse look, and big powerups respawn with dmflags2 bit 27. On Windows
// 2.8.0 keeps the files in the known folders when it cannot write the program folder. compatflags2 bits 2 and 3 come
// in 2.8.0, and the strict modes set bit 3.
constexpr Era ZDOOM_2_7{.gains = programFolderOnly, .line = PortLine::zdoom26};

// Off Windows the settings and saves move from ~/.zdoom to ~/.config/zdoom from 2.7.0.
constexpr Era ZDOOM_2_6 = older(ZDOOM_2_7, {.gains = homeDotFolder});

// compatflags2, compatflags bits 30 and 31 and dmflags2 bit 26 are read from 2.6.0.
constexpr Era ZDOOM_2_5 = older(ZDOOM_2_6, {.lacks = compatflags2, .line = PortLine::zdoom25});

}

constexpr Traits GZDOOM_TRAITS = defined({
        .features = ZDOOM_FAMILY | netmode | teamPick | pistolStart | dmflags3 | keepsCompatflags | stdoutSwitch
                    | GZDOOM_SAVES,
        .changes = GZDOOM_CHANGES,
        .dialect = Dialect::gzdoom,
});

constexpr Traits UZDOOM_TRAITS = defined({
        .features = ZDOOM_FAMILY | teamPick | pistolStart | netgameFolder | coopPreset | dmflags3 | keepsCompatflags
                    | GZDOOM_SAVES,
        .changes = UZDOOM_CHANGES,
        .dialect = Dialect::uzdoom,
        .line = PortLine::uzdoom,
});

constexpr Traits ZDOOM_TRAITS = defined({
        .features = ZDOOM_FAMILY | netmode | teamPick | keepsCompatflags | signedWords | stdoutSwitch | corpseGibs,
        .changes = ZDOOM_28_CHANGES,
        .dialect = Dialect::zdoom,
        .loads = SaveNames::path,
        .line = PortLine::zdoom28,
});

// ZDoom 2.8 underneath, with flag words of its own and +team only once a client is in.
constexpr Traits ZANDRONUM_TRAITS = defined({
        .features = ZDOOM_FAMILY | zadmflags | lmsWeapons | lmsSpectators | zacompatflags | signedWords | stdoutSwitch
                    | corpseGibs | hostsServer | clientDemo,
        .changes = ZANDRONUM_CHANGES,
        .dialect = Dialect::zandronum,
        .loads = SaveNames::path,
        .line = PortLine::zandronum32,
});

// ZDoom 2.3 underneath, with no Zandronum words. dmflags3 and both compatibility words hold bits of its own.
constexpr Traits SKULLTAG_TRAITS = [] {
    Traits skulltag = ZANDRONUM_TRAITS;
    skulltag.features &= ~(zadmflags | zacompatflags | corpseGibs | colorSet);
    skulltag.features |= dmflags3 | programFolderOnly | homeDotFolder | savesWhereRun;
    skulltag.changes = SKULLTAG_CHANGES;
    skulltag.line = PortLine::skulltag;

    return defined(skulltag);
}();

// GZDoom is the guess for a name nothing matches.
constexpr Traits UNKNOWN_TRAITS = [] {
    Traits guessed = GZDOOM_TRAITS;
    guessed.features &= ~(recognised | stdoutSwitch);

    return guessed;
}();

// sv_pistolstart is read from 4.12.1.
constexpr Era GZDOOM_4_11{.lacks = pistolStart};

// dmflags3 is read from 4.11.0, and off Windows the saves move into savegames.
constexpr Era GZDOOM_4_9 = older(GZDOOM_4_11, {.lacks = dmflags3 | savegamesSubfolder});

// From 4.9.0 -loadgame names a save in a folder per game, and on Windows only a portable ini keeps saves beside the
// exe.
constexpr Era GZDOOM_4_8 = older(GZDOOM_4_9, {.lacks = folderPerGame | portableNeedsIni, .special = loadsByPath});

// compatflags bit 25 raises Arch-vile ghosts from 4.8.0.
constexpr Era GZDOOM_4_4 = older(GZDOOM_4_8, {.gains = corpseGibs});

// vid_fullscreen is read from 4.4.0, and saves beside the exe go in a Save folder.
constexpr Era GZDOOM_4_3_3 = older(GZDOOM_4_4, {.lacks = windowsSaveSubfolder, .changes = ZDOOM_28_CHANGES});

// The last range is named by its last release, since nothing opens it from below.
constexpr std::array GZDOOM_RANGES = std::to_array<VersionRange>({
        row("4.12.1", "4.12.1 and later", NEWEST),
        row("4.11.0", "4.11.0 to 4.12.0", GZDOOM_4_11),
        row("4.9.0", "4.9.0 to 4.10.0", GZDOOM_4_9),
        row("4.8.0", "4.8.0 to 4.8.2", GZDOOM_4_8),
        row("4.4.0", "4.4.0 to 4.7.1", GZDOOM_4_4),
        row("4.3.3", "4.3.3 and earlier", GZDOOM_4_3_3),
});

constexpr Releases<GZDOOM_RANGES.size()> GZDOOM_RELEASES(GZDOOM_TRAITS, GZDOOM_RANGES);

static_assert(std::ranges::all_of(GZDOOM_RELEASES.rows,
                                  [](const VersionRange &each) {
                                      const Traits traits = atRelease(GZDOOM_TRAITS, each);

                                      return traits.has(folderPerGame) != (traits.loads == SaveNames::path);
                                  }),
              "a release without a folder per game loads by path");

// 5.0.0 drops -netmode, adds -coop, keeps netgame saves in a folder of their own and hosts up to 64.
constexpr Era UZDOOM_4_14_3{.lacks = netgameFolder | coopPreset, .gains = netmode, .changes = GZDOOM_CHANGES};

constexpr std::array UZDOOM_RANGES = std::to_array<VersionRange>({
        row("5.0.0", "5.0.0 and later", NEWEST),
        row("4.14.3", "4.14.3", UZDOOM_4_14_3),
});

constexpr Releases<UZDOOM_RANGES.size()> UZDOOM_RELEASES(UZDOOM_TRAITS, UZDOOM_RANGES);

// sv_pistolstart is read from 25.6.0, and off Windows the saves move into savegames.
constexpr Era VKDOOM_0_9{.lacks = pistolStart | savegamesSubfolder};

constexpr std::array VKDOOM_RANGES = std::to_array<VersionRange>({
        row("25.6.0", "25.6.0 and later", NEWEST),
        row("0.9", "0.9", VKDOOM_0_9),
});

// VKDoom follows GZDoom, and the newest VKDoom reads all GZDoom does.
constexpr Releases<VKDOOM_RANGES.size()> VKDOOM_RELEASES(GZDOOM_TRAITS, VKDOOM_RANGES);

// dmflags3 and save_dir are read from 0.98c, and the saves move from ~/.skulltag on Linux and the program folder on
// Windows to the folder the port runs in.
constexpr Era SKULLTAG_0_98A{.lacks = dmflags3 | savesWhereRun | saveDirSetting};

// -noautoload, -shotdir, -stdout and compatflags2 are read from 0.98a. Before it some flag bits are missing but none
// mean other things.
constexpr Era SKULLTAG_0_97D = older(SKULLTAG_0_98A, {.lacks = noAutoload | shotdir | stdoutSwitch | compatflags2});

// Each row starts at the first release it describes. An older release runs as the oldest row.
constexpr std::array SKULLTAG_RANGES = std::to_array<VersionRange>({
        row("0.98c", "0.98c to 0.98d", NEWEST),
        row("0.98a", "0.98a to 0.98b", SKULLTAG_0_98A),
        row("0.97d", "0.97d to 0.97d5", SKULLTAG_0_97D),
});

constexpr Releases<SKULLTAG_RANGES.size()> SKULLTAG_RELEASES(SKULLTAG_TRAITS, SKULLTAG_RANGES);

// zadmflags bits 23 to 28 and lmsspectatorsettings bit 2 are read from 3.2.
constexpr Era ZANDRONUM_3_1{.line = PortLine::zandronum31};

// The colorset cvar, zadmflags bits 18 to 22 and zacompatflags bits 13 and 14 are read from 3.1. From 3.1 the cvar of
// zadmflags bit 8 is sv_forcevideodefaults, and sv_forcegldefaults stays as another name.
constexpr Era ZANDRONUM_3_0 = older(ZANDRONUM_3_1, {.lacks = colorSet, .line = PortLine::zandronum30});

constexpr std::array ZANDRONUM_RANGES = std::to_array<VersionRange>({
        row("3.2", "3.2 and later", NEWEST),
        row("3.1", "3.1", ZANDRONUM_3_1),
        row("3.0", "3.0 to 3.0.1", ZANDRONUM_3_0),
});

constexpr Releases<ZANDRONUM_RANGES.size()> ZANDRONUM_RELEASES(ZANDRONUM_TRAITS, ZANDRONUM_RANGES);

constexpr std::array ZDOOM_RANGES = std::to_array<VersionRange>({
        row("2.8.0", "2.8.0 and later", NEWEST),
        row("2.7.0", "2.7.0 to 2.7.1", ZDOOM_2_7),
        row("2.6.0", "2.6.0 to 2.6.1", ZDOOM_2_6),
        row("2.5.0", "2.5.0 and earlier", ZDOOM_2_5),
});

constexpr Releases<ZDOOM_RANGES.size()> ZDOOM_RELEASES(ZDOOM_TRAITS, ZDOOM_RANGES);

}
