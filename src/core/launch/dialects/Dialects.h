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
#pragma once

#include <optional>
#include <span>
#include <string_view>

#include "core/launch/Dialect.h"

namespace Dialect {

// What a netgame of each kind offers, so a dialect says only which kind it plays.
inline constexpr Features NETGAME_ZDOOM = Feature::hosts | Feature::joins | Feature::players | Feature::fragLimit
                                          | Feature::timeLimitCvar | Feature::savegame | Feature::extratic
                                          | Feature::dup | Feature::player | Feature::teams;
inline constexpr Features NETGAME_CHOCOLATE = Feature::hosts | Feature::joins | Feature::listing | Feature::dup;
inline constexpr Features NETGAME_PRBOOM = Feature::joins;

// The bits a dialect of the ZDoom kind decides one by one.
inline constexpr Features NETGAME_ZDOOM_OWN = Feature::netmode | Feature::teamPick | Feature::netgameFolder;

inline constexpr Features NETGAME_DERIVED = NETGAME_ZDOOM | NETGAME_CHOCOLATE | NETGAME_PRBOOM | Feature::deathmatch3;

// Works out what follows from the values: the netgame features from the kind, the save folder
// from the switch, and no demo option without demos.
[[nodiscard]] constexpr Port defined(Port port) {
    Features now = port.features & ~NETGAME_DERIVED;

    switch (port.netgame) {
        case Netgames::zdoom:
            now |= NETGAME_ZDOOM;
            break;
        case Netgames::chocolate:
            now |= NETGAME_CHOCOLATE;

            if (!port.deathmatch3.empty()) {
                now |= Feature::deathmatch3;
            }

            break;
        case Netgames::prboom:
            now |= NETGAME_PRBOOM;
            break;
        case Netgames::none:
            break;
    }

    if (port.netgame != Netgames::zdoom) {
        now &= ~NETGAME_ZDOOM_OWN;
    }

    if (port.save.empty()) {
        now &= ~Features(Feature::saveFolder);
    } else {
        now |= Feature::saveFolder;
    }

    if (!now.has(Feature::demos)) {
        now &= ~(Feature::timedemo | Feature::fastdemo | Feature::continues | Feature::longtics | Feature::soloNet);
    }

    port.features = now;

    return port;
}

[[nodiscard]] constexpr Port atRelease(Port port, const VersionRange &release) {
    port.features = (port.features & ~release.lacks) | release.gains;
    port.withoutRules |= release.withoutRules;

    if (release.special != nullptr) {
        release.special(port);
    }

    return port;
}

}

namespace Dialect::dialects {

// The program name as Dialect::of reads it: lower case, without the extension.
struct Name {
    std::string_view text;

    // A Flatpak export, which no DOS release comes as.
    bool exported;
};

// The port this dialect speaks for the name, or nothing when the name is not one of its own.
using Matcher = std::optional<Port> (*)(const Name &name);

struct Definition {
    Id id;

    // What the dialect speaks when the program name does not name one of its ports.
    Port bare;

    Matcher named;

    // Newest first. Ranges apply to the ports on the line of bare, so a fork on a line of its own has none.
    std::span<const VersionRange> releases;

    // The -complevel numbers it accepts, -1 first.
    std::span<const int> levels;

    std::string_view levelSwitch;

    // The level as it goes on the command line. Null for the number.
    std::string_view (*levelWord)(int level);
};

extern const Definition GZDOOM;
extern const Definition UZDOOM;
extern const Definition ZDOOM;
extern const Definition ZANDRONUM;
extern const Definition DSDA;
extern const Definition PRBOOM;
extern const Definition WOOF;
extern const Definition CHOCOLATE;
extern const Definition ETERNITY;
extern const Definition HELION;
extern const Definition BOOM;
extern const Definition LEGACY;
extern const Definition DOOM;

// What a game version is called, such as Ultimate Doom. Empty for any other level.
[[nodiscard]] std::string_view gameVersionName(int level);

}
