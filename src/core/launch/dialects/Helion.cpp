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
#include <string_view>

#include "core/launch/dialects/Dialects.h"

namespace Dialect::dialects {

namespace {

// -1 first, the port's own.
constexpr std::array HELION_LEVELS = {-1, 2, 9, MBF, MBF21};

// -fast is read only from 0.9.9.1. The cvar works in every release and reads as off without the 1.
constexpr std::array HELION_CHANGES = std::to_array<Spelling>({
        {complevel, {"+complevel"}},
        {fast, {"+sv_fastmonsters", {}, "1"}},
});

// +complevel is read from 0.9.9.0.
constexpr Era HELION_0_9_8{.lacks = complevel};

// -solo-net is read from 0.9.8.0.
constexpr Era HELION_0_9_7{.lacks = HELION_0_9_8.lacks | soloNet};

// From 0.9.7.0 the saves go in the user data folder.
constexpr Era HELION_0_9_4{.lacks = HELION_0_9_7.lacks, .gains = savesBesideProgram};

// -pistolstart and -nomusic are read from 0.9.4.0.
constexpr Era HELION_0_9_2_9{.lacks = HELION_0_9_4.lacks | pistolStart | nomusic, .gains = savesBesideProgram};

// -savedir is read from 0.9.2.9.
constexpr Era HELION_0_9_2_4{.lacks = HELION_0_9_2_9.lacks | saveFolder, .gains = savesBesideProgram};

// -config is read from 0.9.2.4. Helion 0.9.0.0 and 0.9.1.0 save in the folder they run in, listed here as the
// program folder.
constexpr Era HELION_0_9_0{.lacks = HELION_0_9_2_4.lacks | configFile, .gains = savesBesideProgram};

std::string_view helionWord(const int level) {
    switch (level) {
        case 2:
            return "vanilla";
        case 9:
            return "boom";
        case MBF:
            return "mbf";
        case MBF21:
            return "mbf21";
        default:
            return {};
    }
}

}

constexpr Traits HELION_TRAITS = defined({
        .features = COMMON | recognised | iwad | configFile | saveFolder | deh | bex | onePatch | map | longWarp
                    | loadgame | replays | soloNet | levelstat | pistolStart | nomusic | displayKept,
        .saveExt = ".hsg",
        .configName = "helion",
        .changes = HELION_CHANGES,
        .dialect = Dialect::helion,
        .loads = SaveNames::name,
        .netgame = Netgames::none,
        .complevel = Complevels::helion,
        .line = PortLine::helion,
});

// Each row starts at the first release it describes. From 0.9.1.2 Helion reads an unknown switch as a value of the
// switch before it, so an older release must get none of the newer ones.
constexpr std::array HELION_RANGES = std::to_array<VersionRange>({
        row("0.9.9.0", "0.9.9.0 and later", NEWEST),
        row("0.9.8.0", "0.9.8.0", HELION_0_9_8),
        row("0.9.7.0", "0.9.7.0", HELION_0_9_7),
        row("0.9.4.0", "0.9.4.0 to 0.9.6.1", HELION_0_9_4),
        row("0.9.2.9", "0.9.2.9 to 0.9.3.0", HELION_0_9_2_9),
        row("0.9.2.4", "0.9.2.4 to 0.9.2.8", HELION_0_9_2_4),
        row("0.9.0.0", "0.9.0.0 to 0.9.2.3", HELION_0_9_0),
});

constexpr Releases<HELION_RANGES.size()> HELION_RELEASES(HELION_TRAITS, HELION_RANGES);

constexpr Numbering HELION_NUMBERING{.levels = HELION_LEVELS, .levelWord = helionWord};

}
