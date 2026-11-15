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

constexpr Port WOOF_PORT = defined({
        .features = Feature::recognised | Feature::iwad | Feature::configFile | Feature::respawn | Feature::longWarp
                    | Feature::loadgame | Feature::demos | Feature::timedemo | Feature::fastdemo
                    | Feature::recordsWithComplevel | Feature::recordsWithSkill | Feature::longtics | Feature::soloNet
                    | Feature::recordAddsLmp | Feature::continues | Feature::continueComplevel | Feature::levelstat
                    | Feature::pistolStart | Feature::nomusic | Feature::displayKept | Feature::shotdir,
        .save = "-save",
        .saveExt = ".dsg",
        .autosave = "autosave.dsg",
        .periodicAutosave = "autosav",
        .configName = "woof",
        .bex = "-deh",
        .deathmatch3 = "-dm3",
        .unrecordable = ID24,
        .id = Id::woof,
        .loads = SaveNames::slot,
        .netgame = Netgames::chocolate,
        .complevel = Complevels::woof,
        .pistolStart = PistolStart::flag,
        .display = Windowing::switches,
        .line = PortLine::woof,
});

constexpr std::array NAMES = {"woof", "nugget", "cherry"};

// Nugget and Cherry write settings that Woof would drop.
std::optional<Port> woofNamed(const Name &name) {
    for (const char *each : NAMES) {
        if (!name.text.contains(each)) {
            continue;
        }

        Port woof = WOOF_PORT;
        woof.configName = each;

        // Cherry Doom records without a skill.
        if (std::string_view(each) == "cherry") {
            woof.features &= ~Features(Feature::recordsWithSkill);
        }

        return woof;
    }

    return std::nullopt;
}

}

constexpr Definition WOOF{
        .id = Id::woof,
        .bare = WOOF_PORT,
        .named = woofNamed,
        .releases = {},
        .levels = WOOF_LEVELS,
        .levelSwitch = "-complevel",
        .levelWord = nullptr,
};

}
