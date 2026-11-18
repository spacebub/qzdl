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
// 2.8.0 keeps the files in the known folders when it cannot write the program folder.
constexpr Era ZDOOM_2_7{.gains = twoStateFreelook | programFolderOnly};

// Off Windows the settings and saves move from ~/.zdoom to ~/.config/zdoom from 2.7.0.
constexpr Era ZDOOM_2_6 = older(ZDOOM_2_7, {.gains = homeDotFolder});

// compatflags2 is read from 2.6.0.
constexpr Era ZDOOM_2_5 = older(ZDOOM_2_6, {.lacks = compatflags2});

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
        .line = PortLine::zdoom,
});

// ZDoom 2.8 underneath, with flag words of its own and +team only once a client is in.
constexpr Traits ZANDRONUM_TRAITS = defined({
        .features = ZDOOM_FAMILY | zadmflags | lmsWeapons | lmsSpectators | zacompatflags | signedWords | stdoutSwitch
                    | corpseGibs | twoStateFreelook | hostsServer | clientDemo,
        .changes = ZANDRONUM_CHANGES,
        .dialect = Dialect::zandronum,
        .loads = SaveNames::path,
        .line = PortLine::zandronum,
});

// ZDoom 2.3 underneath, with no Zandronum words. dmflags3 and both compatibility words hold bits of its own.
constexpr Traits SKULLTAG_TRAITS = [] {
    Traits skulltag = ZANDRONUM_TRAITS;
    skulltag.features &= ~(zadmflags | zacompatflags | corpseGibs | twoStateFreelook | colorSet);
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

// The last range is named by its last release, since nothing opens it from below.
constexpr std::array GZDOOM_RANGES = std::to_array<VersionRange>({
        {
                .first = "4.12.1",
                .label = "4.12.1 and later",
                .lacks = {},
                .gains = {},
                .changes = {},
                .special = nullptr,
        },
        // sv_pistolstart is read from 4.12.1.
        {
                .first = "4.11.0",
                .label = "4.11.0 to 4.12.0",
                .lacks = pistolStart,
                .gains = {},
                .changes = {},
                .special = nullptr,
        },
        // dmflags3 is read from 4.11.0, and off Windows the saves move into savegames.
        {
                .first = "4.9.0",
                .label = "4.9.0 to 4.10.0",
                .lacks = pistolStart | dmflags3 | savegamesSubfolder,
                .gains = {},
                .changes = {},
                .special = nullptr,
        },
        // From 4.9.0 -loadgame names a save in a folder per game, and on Windows only a portable ini keeps saves
        // beside the exe.
        {
                .first = "4.8.0",
                .label = "4.8.0 to 4.8.2",
                .lacks = pistolStart | dmflags3 | savegamesSubfolder | folderPerGame | portableNeedsIni,
                .gains = {},
                .changes = {},
                .special = loadsByPath,
        },
        // compatflags bit 25 raises Arch-vile ghosts from 4.8.0.
        {
                .first = "4.4.0",
                .label = "4.4.0 to 4.7.1",
                .lacks = pistolStart | dmflags3 | savegamesSubfolder | folderPerGame | portableNeedsIni,
                .gains = corpseGibs,
                .changes = {},
                .special = loadsByPath,
        },
        // vid_fullscreen is read from 4.4.0, and saves beside the exe go in a Save folder.
        {
                .first = "4.3.3",
                .label = "4.3.3 and earlier",
                .lacks = pistolStart | dmflags3 | savegamesSubfolder | folderPerGame | portableNeedsIni
                         | windowsSaveSubfolder,
                .gains = corpseGibs,
                .changes = ZDOOM_28_CHANGES,
                .special = loadsByPath,
        },
});

constexpr Releases<GZDOOM_RANGES.size()> GZDOOM_RELEASES(GZDOOM_TRAITS, GZDOOM_RANGES);

static_assert(std::ranges::all_of(GZDOOM_RELEASES.rows,
                                  [](const VersionRange &each) {
                                      const Traits traits = atRelease(GZDOOM_TRAITS, each);

                                      return traits.has(folderPerGame) != (traits.loads == SaveNames::path);
                                  }),
              "a release without a folder per game loads by path");

constexpr std::array UZDOOM_RANGES = std::to_array<VersionRange>({
        {
                .first = "5.0.0",
                .label = "5.0.0 and later",
                .lacks = {},
                .gains = {},
                .changes = {},
                .special = nullptr,
        },
        // 5.0.0 drops -netmode, adds -coop, keeps netgame saves in a folder of their own and hosts up to 64.
        {
                .first = "4.14.3",
                .label = "4.14.3",
                .lacks = netgameFolder | coopPreset,
                .gains = netmode,
                .changes = GZDOOM_CHANGES,
                .special = nullptr,
        },
});

constexpr Releases<UZDOOM_RANGES.size()> UZDOOM_RELEASES(UZDOOM_TRAITS, UZDOOM_RANGES);

constexpr std::array VKDOOM_RANGES = std::to_array<VersionRange>({
        {
                .first = "25.6.0",
                .label = "25.6.0 and later",
                .lacks = {},
                .gains = {},
                .changes = {},
                .special = nullptr,
        },
        // sv_pistolstart is read from 25.6.0, and off Windows the saves move into savegames.
        {
                .first = "0.9",
                .label = "0.9",
                .lacks = pistolStart | savegamesSubfolder,
                .gains = {},
                .changes = {},
                .special = nullptr,
        },
});

// VKDoom follows GZDoom, and the newest VKDoom reads all GZDoom does.
constexpr Releases<VKDOOM_RANGES.size()> VKDOOM_RELEASES(GZDOOM_TRAITS, VKDOOM_RANGES);

// Each row starts at the first release it describes. An older release runs as the oldest row.
constexpr std::array SKULLTAG_RANGES = std::to_array<VersionRange>({
        {
                .first = "0.98c",
                .label = "0.98c to 0.98d",
                .lacks = {},
                .gains = {},
                .changes = {},
                .special = nullptr,
        },
        // dmflags3 and save_dir are read from 0.98c, and the saves move from ~/.skulltag on Linux and the program
        // folder on Windows to the folder the port runs in.
        {
                .first = "0.98a",
                .label = "0.98a to 0.98b",
                .lacks = dmflags3 | savesWhereRun | saveDirSetting,
                .gains = {},
                .changes = {},
                .special = nullptr,
        },
        // -noautoload, -shotdir, -stdout and compatflags2 are read from 0.98a. Before it some flag bits are missing
        // but none mean other things.
        {
                .first = "0.97d",
                .label = "0.97d to 0.97d5",
                .lacks = dmflags3 | savesWhereRun | saveDirSetting | noAutoload | shotdir | stdoutSwitch | compatflags2,
                .gains = {},
                .changes = {},
                .special = nullptr,
        },
});

constexpr Releases<SKULLTAG_RANGES.size()> SKULLTAG_RELEASES(SKULLTAG_TRAITS, SKULLTAG_RANGES);

constexpr std::array ZANDRONUM_RANGES = std::to_array<VersionRange>({
        row("3.1", "3.1 and later", NEWEST),
        // The colorset cvar is read from 3.1.
        row("3.0", "3.0 to 3.0.1", Era{.lacks = colorSet}),
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
