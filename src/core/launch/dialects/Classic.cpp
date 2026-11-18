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

constexpr std::array BOOM_CHANGES = std::to_array<Spelling>({
        {saveFolder, {"-save"}},
        // MBF skips its preloaded files. Boom has none, and passes over a switch it does not know.
        {noAutoload, {"-noload"}},
});

constexpr std::array LEGACY_CHANGES = std::to_array<Spelling>({
        {deh, {"-dehacked"}},
        {bex, {"-dehacked"}},
        // Its -nosound leaves the music on.
        {nosfx, {"-nosound"}},
});

}

constexpr Traits BOOM_TRAITS = defined({
        .features = COMMON | recognised | iwad | saveFolder | deh | bex | noAutoload | nosfx | respawn | loadgame
                    | replays | timedemo | fastdemo | recordsBareName | nomusic | displayKept,
        .saveExt = ".dsg",
        .configName = "mbf",
        .changes = BOOM_CHANGES,
        .dialect = Dialect::boom,
        .loads = SaveNames::slot,
        .netgame = Netgames::none,
        .line = PortLine::none,
});

// MBF takes -config. Boom 2.02 keeps boom.cfg beside the exe.
constexpr Traits MBF_TRAITS = [] {
    Traits mbf = BOOM_TRAITS;
    mbf.features |= configFile;

    return defined(mbf);
}();

// Takes -config but writes to it without ever reading it back, so it keeps
// $DOOMWADDIR/config.cfg instead.
constexpr Traits LEGACY_TRAITS = defined({
        .features = COMMON | recognised | deh | bex | nosfx | respawn | loadgame | replays | timedemo | recordsBareName
                    | nomusic | displayKept,
        .saveExt = ".dsg",
        .changes = LEGACY_CHANGES,
        .dialect = Dialect::legacy,
        .loads = SaveNames::slot,
        .netgame = Netgames::none,
        .line = PortLine::none,
});

constexpr Traits DOOM_TRAITS = defined({
        .features = COMMON | recognised | nosfx | respawn | loadgame | replays | timedemo | recordsBareName | nomusic
                    | displayKept,
        .saveExt = ".dsg",
        .dialect = Dialect::doom,
        .loads = SaveNames::slot,
        .netgame = Netgames::none,
        .line = PortLine::none,
});

}
