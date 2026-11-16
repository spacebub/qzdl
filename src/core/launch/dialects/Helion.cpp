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
constexpr std::array HELION_LEVELS = {-1, 2, 9, MBF, MBF21};

constexpr Port HELION_PORT = defined({
        .features = Feature::recognised | Feature::iwad | Feature::configFile | Feature::map | Feature::longWarp
                    | Feature::loadgame | Feature::demos | Feature::soloNet | Feature::levelstat | Feature::pistolStart
                    | Feature::nomusic | Feature::displayKept,
        .saveExt = ".hsg",
        .configName = "helion",
        .bex = "-deh",
        .noAutoload = "",
        .nosfx = "",
        .id = Id::helion,
        .loads = SaveNames::name,
        .netgame = Netgames::none,
        .complevel = Complevels::helion,
        .display = Windowing::none,
        .line = PortLine::helion,
});

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

std::optional<Port> helionNamed(const Name &name) {
    if (name.text.contains("helion")) {
        return HELION_PORT;
    }

    return std::nullopt;
}

}

constexpr Definition HELION{
        .id = Id::helion,
        .bare = HELION_PORT,
        .named = helionNamed,
        .releases = {},
        .levels = HELION_LEVELS,
        .levelSwitch = "+complevel",
        .levelWord = helionWord,
};

}
