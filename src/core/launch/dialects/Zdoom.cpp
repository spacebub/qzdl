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
                                  | fullscreen | windowed | exec | map | respawn | loadgame | demos | timedemo
                                  | recordAddsLmp | cheats | nomusic | displayKept | shotdir | alwaysApplyDmflags
                                  | turbo | dmflags | dmflags2 | compatflags | compatflags2;

constexpr std::array GZDOOM_CHANGES = std::to_array<Spelling>({
        // The GZDoom line takes pistol start as a cvar, which no demo or netgame refuses.
        {pistolStart, {"+sv_pistolstart", "Starts every map with only the pistol.", "1"}},
        {fullscreen, {"+vid_fullscreen", {}, "1"}},
        {windowed, {"+vid_fullscreen", {}, "0"}},
        {bex, {"-bex"}},
});

constexpr std::array ZDOOM_28_CHANGES = std::to_array<Spelling>({
        {fullscreen, {"+fullscreen", {}, "1"}},
        {windowed, {"+fullscreen", {}, "0"}},
        {bex, {"-bex"}},
});

// Where the GZDoom line keeps saves and settings.
constexpr Features GZDOOM_SAVES =
        folderPerGame | savegamesSubfolder | windowsSaveSubfolder | portableIni | portableNeedsIni;

// -loadgame takes the path it is given.
constexpr void loadsByPath(Port &port) {
    port.loads = SaveNames::path;
}

}

constexpr Port GZDOOM_PORT = defined({
        .features = ZDOOM_FAMILY | netmode | teamPick | pistolStart | dmflags3 | keepsCompatflags | stdoutSwitch
                    | GZDOOM_SAVES,
        .changes = GZDOOM_CHANGES,
        .dialect = Dialect::gzdoom,
});

constexpr Port UZDOOM_PORT = defined({
        .features = ZDOOM_FAMILY | teamPick | pistolStart | netgameFolder | coopPreset | dmflags3 | keepsCompatflags
                    | GZDOOM_SAVES,
        .changes = GZDOOM_CHANGES,
        .dialect = Dialect::uzdoom,
        .line = PortLine::uzdoom,
});

constexpr Port ZDOOM_PORT = defined({
        .features = ZDOOM_FAMILY | netmode | teamPick | keepsCompatflags | signedWords | stdoutSwitch | corpseGibs,
        .changes = ZDOOM_28_CHANGES,
        .dialect = Dialect::zdoom,
        .loads = SaveNames::path,
        .line = PortLine::zdoom,
});

// ZDoom 2.8 underneath, with flag words of its own and +team only once a client is in.
constexpr Port ZANDRONUM_PORT = defined({
        .features = ZDOOM_FAMILY | netmode | zadmflags | lmsWeapons | lmsSpectators | zacompatflags | signedWords
                    | stdoutSwitch | corpseGibs,
        .changes = ZDOOM_28_CHANGES,
        .dialect = Dialect::zandronum,
        .loads = SaveNames::path,
        .line = PortLine::zandronum,
});

// GZDoom is the guess for a name nothing matches.
constexpr Port UNKNOWN_PORT = [] {
    Port guessed = GZDOOM_PORT;
    guessed.features &= ~(recognised | stdoutSwitch);

    return guessed;
}();

// The last range is named by its last release, since nothing opens it from below.
constexpr std::array GZDOOM_RELEASES = std::to_array<VersionRange>({
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

static_assert(soundAtEvery(GZDOOM_PORT, GZDOOM_RELEASES), "every release is sound");
static_assert(againstLatest(GZDOOM_PORT, GZDOOM_RELEASES), "every release is stated against the latest");
static_assert(std::ranges::all_of(GZDOOM_RELEASES,
                                  [](const VersionRange &each) {
                                      const Port port = atRelease(GZDOOM_PORT, each);

                                      return port.has(folderPerGame) != (port.loads == SaveNames::path);
                                  }),
              "a release without a folder per game loads by path");

constexpr std::array UZDOOM_RELEASES = std::to_array<VersionRange>({
        {
                .first = "5.0.0",
                .label = "5.0.0 and later",
                .lacks = {},
                .gains = {},
                .changes = {},
                .special = nullptr,
        },
        // 5.0.0 drops -netmode, adds -coop and keeps netgame saves in a folder of their own.
        {
                .first = "4.14.3",
                .label = "4.14.3",
                .lacks = netgameFolder | coopPreset,
                .gains = netmode,
                .changes = {},
                .special = nullptr,
        },
});

static_assert(soundAtEvery(UZDOOM_PORT, UZDOOM_RELEASES), "every release is sound");
static_assert(againstLatest(UZDOOM_PORT, UZDOOM_RELEASES), "every release is stated against the latest");

}
