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
#include <cstddef>

#include "core/launch/dialects/Dialects.h"

namespace Dialect::dialects {

namespace {

// -1 first, the port's own.
constexpr std::array PRBOOM_LEVELS = {-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, MBF, 12, 13, 14, 15, 16, 17};
constexpr std::array DSDA_LEVELS = {-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, MBF, 12, 13, 14, 15, 16, 17, MBF21};

constexpr Port PRBOOM_PORT = defined({
        .features = Feature::recognised | Feature::iwad | Feature::configFile | Feature::respawn | Feature::longWarp
                    | Feature::loadgame | Feature::demos | Feature::timedemo | Feature::fastdemo | Feature::longtics
                    | Feature::soloNet | Feature::recordAddsLmp | Feature::continues | Feature::levelstat
                    | Feature::pistolStart | Feature::nomusic | Feature::shotdir,
        .save = "-save",
        .saveExt = ".dsg",
        .configName = "prboom",
        .bex = "-deh",
        .noAutoload = "-noload",
        .id = Id::prboom,
        .loads = SaveNames::slot,
        .netgame = Netgames::prboom,
        .complevel = Complevels::prboom,
        .pistolStart = PistolStart::flag,
        .display = Windowing::noWindow,
        .line = PortLine::prboom,
});

constexpr Port DSDA_PORT = defined({
        .features = Feature::recognised | Feature::iwad | Feature::configFile | Feature::respawn | Feature::longWarp
                    | Feature::loadgame | Feature::demos | Feature::timedemo | Feature::fastdemo
                    | Feature::recordsWithComplevel | Feature::recordsWithSkill | Feature::longtics | Feature::soloNet
                    | Feature::recordAddsLmp | Feature::continues | Feature::continueComplevel | Feature::levelstat
                    | Feature::pistolStart | Feature::nomusic | Feature::shotdir,
        .save = "-save",
        .saveExt = ".dsg",
        .configName = "dsda",
        .bex = "-deh",
        .lastSlot = 118,
        .id = Id::dsda,
        .loads = SaveNames::slot,
        .netgame = Netgames::none,
        .complevel = Complevels::dsda,
        .pistolStart = PistolStart::flag,
        .display = Windowing::switches,
        .line = PortLine::dsda,
});

// A DSDA-Doom fork with its own config.
constexpr Port NYAN_PORT = [] {
    Port nyan = DSDA_PORT;
    nyan.configName = "nyan";
    nyan.line = PortLine::nyan;

    return nyan;
}();

constexpr Port ETERNITY_PORT = defined({
        .features = Feature::recognised | Feature::iwad | Feature::configFile | Feature::respawn | Feature::longWarp
                    | Feature::loadgame | Feature::demos | Feature::timedemo | Feature::fastdemo | Feature::soloNet
                    | Feature::recordAddsLmp | Feature::nomusic | Feature::displayKept,
        .save = "-save",
        .saveExt = ".dsg",
        .configName = "eternity",
        .noAutoload = "-noload",
        .id = Id::eternity,
        .loads = SaveNames::slot,
        .netgame = Netgames::none,
        .display = Windowing::none,
        .line = PortLine::eternity,
});

// A DSDA-Doom release such as 0.27.1 as 2701.
constexpr int releaseOf(const std::string_view version) {
    const size_t dot = version.find('.', 2);
    int minor = 0;
    int patch = 0;

    for (const char each : version.substr(2, dot - 2)) {
        minor = (minor * 10) + (each - '0');
    }

    for (const char each : version.substr(dot + 1)) {
        patch = (patch * 10) + (each - '0');
    }

    return (minor * 100) + patch;
}

// Newest first, each range against the 0.30.0 release the port above describes. The last range is
// named by its last release, since nothing opens it from below.
constexpr std::array DSDA_RELEASES = std::to_array<VersionRange>({
        {
                .first = "0.30.0",
                .label = "0.30.0 and later",
                .lacks = {},
                .gains = {},
                .withoutRules = 0,
                .special = nullptr,
        },
        // -loadgame is read from 0.30.0.
        {
                .first = "0.28.0",
                .label = "0.28.0 to 0.29.4",
                .lacks = Feature::loadgame,
                .gains = {},
                .withoutRules = 0,
                .special = nullptr,
        },
        // -recordfromto stops without -skill until 0.28.0.
        {
                .first = "0.27.0",
                .label = "0.27.0 to 0.27.5",
                .lacks = Feature::loadgame,
                .gains = Feature::continuesWithSkill,
                .withoutRules = 0,
                .special = nullptr,
        },
        // -record stops without -skill from 0.27.0.
        {
                .first = "0.26.0",
                .label = "0.26.0 to 0.26.2",
                .lacks = Feature::loadgame | Feature::recordsWithSkill,
                .gains = {},
                .withoutRules = 0,
                .special = nullptr,
        },
        // -chain_episodes and -game_speed are read from 0.26.0.
        {
                .first = "0.25.0",
                .label = "0.25.0 to 0.25.6",
                .lacks = Feature::loadgame | Feature::recordsWithSkill,
                .gains = {},
                .withoutRules = ruleBit(Rules::Rule::chainEpisodes) | ruleBit(Rules::Rule::speed),
                .special = nullptr,
        },
        // -record stops without -complevel from 0.25.0.
        {
                .first = "0.22.3",
                .label = "0.22.3 to 0.24.3",
                .lacks = Feature::loadgame | Feature::recordsWithSkill | Feature::recordsWithComplevel,
                .gains = {},
                .withoutRules = ruleBit(Rules::Rule::chainEpisodes) | ruleBit(Rules::Rule::speed),
                .special = nullptr,
        },
        // -loadgame is ignored from 0.22.3 on.
        {
                .first = "0.21.2",
                .label = "0.21.2 to 0.22.2",
                .lacks = Feature::recordsWithSkill | Feature::recordsWithComplevel,
                .gains = {},
                .withoutRules = ruleBit(Rules::Rule::chainEpisodes) | ruleBit(Rules::Rule::speed),
                .special = nullptr,
        },
        // -pistolstart is read from 0.21.2.
        {
                .first = "0.21.1",
                .label = "0.21.1 and earlier",
                .lacks = Feature::recordsWithSkill | Feature::recordsWithComplevel | Feature::pistolStart,
                .gains = {},
                .withoutRules = ruleBit(Rules::Rule::chainEpisodes) | ruleBit(Rules::Rule::speed),
                .special = nullptr,
        },
});

static_assert(std::ranges::adjacent_find(DSDA_RELEASES,
                                         [](const VersionRange &newer, const VersionRange &older) {
                                             return releaseOf(newer.first) <= releaseOf(older.first);
                                         })
                      == DSDA_RELEASES.end(),
              "releases newest first");

std::optional<Port> prboomNamed(const Name &name) {
    for (const char *each : {"prboom", "glboom", "rude"}) {
        if (name.text.contains(each)) {
            Port boom = PRBOOM_PORT;
            boom.configName = each;

            return boom;
        }
    }

    return std::nullopt;
}

std::optional<Port> dsdaNamed(const Name &name) {
    if (name.text.contains("dsda")) {
        return DSDA_PORT;
    }

    if (name.text.contains("nyan")) {
        return NYAN_PORT;
    }

    return std::nullopt;
}

std::optional<Port> eternityNamed(const Name &name) {
    if (name.text.contains("eternity")) {
        return ETERNITY_PORT;
    }

    return std::nullopt;
}

}

constexpr Definition PRBOOM{
        .id = Id::prboom,
        .bare = PRBOOM_PORT,
        .named = prboomNamed,
        .releases = {},
        .levels = PRBOOM_LEVELS,
        .levelSwitch = "-complevel",
        .levelWord = nullptr,
};

constexpr Definition DSDA{
        .id = Id::dsda,
        .bare = DSDA_PORT,
        .named = dsdaNamed,
        .releases = DSDA_RELEASES,
        .levels = DSDA_LEVELS,
        .levelSwitch = "-complevel",
        .levelWord = nullptr,
};

constexpr Definition ETERNITY{
        .id = Id::eternity,
        .bare = ETERNITY_PORT,
        .named = eternityNamed,
        .releases = {},
        .levels = {},
        .levelSwitch = "-complevel",
        .levelWord = nullptr,
};

}
