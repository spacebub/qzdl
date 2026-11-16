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

constexpr std::array PRBOOM_OWN = std::to_array<Spelling>({
        {saveFolder, {"-save"}},
        {noAutoload, {"-noload"}},
        {fullscreen, {"-nowindow"}},
        {joins, {"-net"}},
});

constexpr std::array PRBOOM_CHANGES = join(TURBO_TO_400, PRBOOM_OWN);

constexpr Port PRBOOM_PORT = defined({
        .features = COMMON | recognised | iwad | configFile | saveFolder | deh | bex | noAutoload | nosfx | fullscreen
                    | windowed | respawn | longWarp | loadgame | demos | timedemo | fastdemo | longtics | soloNet
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
        .program = Program::prboom,
});

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

constexpr Port DSDA_PORT = defined({
        .features = COMMON | recognised | iwad | configFile | saveFolder | deh | bex | noAutoload | nosfx | fullscreen
                    | windowed | respawn | longWarp | loadgame | demos | timedemo | fastdemo | recordsWithComplevel
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
        .program = Program::dsda,
});

// A DSDA-Doom fork with its own config.
constexpr Port NYAN_PORT = [] {
    Port nyan = DSDA_PORT;
    nyan.features |= limitRemoving;
    nyan.configName = "nyan";
    nyan.line = PortLine::nyan;
    nyan.program = Program::nyan;

    return defined(nyan);
}();

constexpr std::array ETERNITY_OWN = std::to_array<Spelling>({
        {saveFolder, {"-save"}},
        {noAutoload, {"-noload"}},
        {bex, {"-bex"}},
        {dmflags, {"-dmflags"}},
});

constexpr std::array ETERNITY_CHANGES = join(TURBO_TO_400, ETERNITY_OWN);

constexpr Port ETERNITY_PORT = defined({
        .features = COMMON | recognised | iwad | configFile | saveFolder | deh | bex | noAutoload | nosfx | respawn
                    | longWarp | loadgame | demos | timedemo | fastdemo | soloNet | recordAddsLmp | nomusic
                    | displayKept | vanillaMode | turbo | gameSpeed | dogs | dmflags,
        .saveExt = ".dsg",
        .configName = "eternity",
        .changes = ETERNITY_CHANGES,
        .dialect = Dialect::eternity,
        .loads = SaveNames::slot,
        .netgame = Netgames::none,
        .line = PortLine::eternity,
        .program = Program::eternity,
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
                .changes = {},
                .special = nullptr,
        },
        // -loadgame is read from 0.30.0.
        {
                .first = "0.28.0",
                .label = "0.28.0 to 0.29.4",
                .lacks = loadgame,
                .gains = {},
                .changes = {},
                .special = nullptr,
        },
        // -recordfromto stops without -skill until 0.28.0.
        {
                .first = "0.27.0",
                .label = "0.27.0 to 0.27.5",
                .lacks = loadgame,
                .gains = continuesWithSkill,
                .changes = {},
                .special = nullptr,
        },
        // -record stops without -skill from 0.27.0.
        {
                .first = "0.26.0",
                .label = "0.26.0 to 0.26.2",
                .lacks = loadgame | recordsWithSkill,
                .gains = {},
                .changes = {},
                .special = nullptr,
        },
        // -chain_episodes and -game_speed are read from 0.26.0.
        {
                .first = "0.25.0",
                .label = "0.25.0 to 0.25.6",
                .lacks = loadgame | recordsWithSkill | chainEpisodes | gameSpeed,
                .gains = {},
                .changes = {},
                .special = nullptr,
        },
        // -record stops without -complevel from 0.25.0.
        {
                .first = "0.22.3",
                .label = "0.22.3 to 0.24.3",
                .lacks = loadgame | recordsWithSkill | recordsWithComplevel | chainEpisodes | gameSpeed,
                .gains = {},
                .changes = {},
                .special = nullptr,
        },
        // -loadgame is ignored from 0.22.3 on.
        {
                .first = "0.21.2",
                .label = "0.21.2 to 0.22.2",
                .lacks = recordsWithSkill | recordsWithComplevel | chainEpisodes | gameSpeed,
                .gains = {},
                .changes = {},
                .special = nullptr,
        },
        // -pistolstart is read from 0.21.2.
        {
                .first = "0.21.1",
                .label = "0.21.1 and earlier",
                .lacks = recordsWithSkill | recordsWithComplevel | pistolStart | chainEpisodes | gameSpeed,
                .gains = {},
                .changes = {},
                .special = nullptr,
        },
});

static_assert(std::ranges::adjacent_find(DSDA_RELEASES,
                                         [](const VersionRange &newer, const VersionRange &older) {
                                             return releaseOf(newer.first) <= releaseOf(older.first);
                                         })
                      == DSDA_RELEASES.end(),
              "releases newest first");
static_assert(soundAtEvery(DSDA_PORT, DSDA_RELEASES), "every release is sound");
static_assert(againstLatest(DSDA_PORT, DSDA_RELEASES), "every release is stated against the latest");

constexpr std::array PRBOOM_FORKS = {
        Fork{.name = "prboom", .program = Program::prboom},
        Fork{.name = "glboom", .program = Program::glboom},
        Fork{.name = "rude", .program = Program::rude},
};

std::optional<Port> prboomNamed(const Name &name) {
    for (const Fork &each : PRBOOM_FORKS) {
        if (name.text.contains(each.name)) {
            Port boom = PRBOOM_PORT;
            boom.configName = each.name;
            boom.program = each.program;

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
        .dialect = Dialect::prboom,
        .bare = PRBOOM_PORT,
        .named = prboomNamed,
        .releases = {},
        .levels = PRBOOM_LEVELS,
        .levelWord = nullptr,
};

constexpr Definition DSDA{
        .dialect = Dialect::dsda,
        .bare = DSDA_PORT,
        .named = dsdaNamed,
        .releases = DSDA_RELEASES,
        .levels = DSDA_LEVELS,
        .levelWord = nullptr,
};

static_assert(keyed(DSDA), "the releases key on a known program");

constexpr Definition ETERNITY{
        .dialect = Dialect::eternity,
        .bare = ETERNITY_PORT,
        .named = eternityNamed,
        .releases = {},
        .levels = {},
        .levelWord = nullptr,
};

}
