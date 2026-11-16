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
constexpr std::array WOOF_LEVELS = {-1, 2, 3, 4, 9, MBF, MBF21, ID24};

// NOLINTBEGIN(readability-trailing-comma): a change is one row, with its description inside it
constexpr std::array WOOF_OWN = std::to_array<Spelling>({
        {saveFolder, {"-save"}},
        {gameSpeed,
         {"-speed",
          "Game speed in percent, from 10 to 1000. Strict mode and netgames keep it at 100.",
          {},
          Range{.least = 10, .most = 1000}}},
        {dogs,
         {"-dogs",
          "How many helper dogs join a single player game, from 0 to 3. Left out below the MBF complevel, since the "
          "port stops there.",
          {},
          Range{.least = 0, .most = 3},
          PLAYBACK,
          Levels{.from = MBF, .to = AND_UP, .onLine = false}}},
});
// NOLINTEND(readability-trailing-comma)

constexpr std::array WOOF_CHANGES = join(CHOCOLATE_CHANGES, WOOF_OWN);

}

constexpr Port WOOF_PORT = defined({
        .features = COMMON | recognised | iwad | configFile | saveFolder | deh | bex | noAutoload | nosfx | fullscreen
                    | windowed | deathmatch3 | respawn | longWarp | loadgame | demos | timedemo | fastdemo
                    | recordsWithComplevel | recordsWithSkill | longtics | soloNet | recordAddsLmp | continues
                    | continueComplevel | levelstat | pistolStart | nomusic | displayKept | shotdir | coopSpawns
                    | noCheats | noOptions | mbfBeta | turbo | gameSpeed | dogs,
        .saveExt = ".dsg",
        .autosave = "autosave.dsg",
        .periodicAutosave = "autosav",
        .configName = "woof",
        .changes = WOOF_CHANGES,
        .unrecordable = ID24,
        .dialect = Dialect::woof,
        .loads = SaveNames::slot,
        .netgame = Netgames::chocolate,
        .complevel = Complevels::woof,
        .line = PortLine::woof,
});

// Nugget and Cherry write settings that Woof would drop.
constexpr Port NUGGET_PORT = withConfig(WOOF_PORT, "nugget");

// Cherry Doom records without a skill.
constexpr Port CHERRY_PORT = [] {
    Port cherry = withConfig(WOOF_PORT, "cherry");
    cherry.features &= ~Features(recordsWithSkill);

    return cherry;
}();

constexpr Numbering WOOF_NUMBERING{
        .complevel = Complevels::woof,
        .levels = WOOF_LEVELS,
        .levelWord = nullptr,
};

}
