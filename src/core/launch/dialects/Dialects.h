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

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <optional>
#include <span>
#include <string_view>

#include "core/config/Options.h"
#include "core/launch/Dialect.h"
#include "core/launch/dialects/Baseline.h"

namespace Dialect {

// What a netgame of each kind offers, so a dialect says only which kind it plays.
inline constexpr Features NETGAME_HOST =
        Feature::hosts | Feature::deathmatch | Feature::altDeathmatch | Feature::netPort | Feature::timeLimit;
inline constexpr Features NETGAME_ZDOOM = NETGAME_HOST | Feature::joins | Feature::players | Feature::fragLimit
                                          | Feature::savegame | Feature::extratic | Feature::dup | Feature::player
                                          | Feature::teams;
inline constexpr Features NETGAME_CHOCOLATE =
        NETGAME_HOST | Feature::joins | Feature::listing | Feature::dup | Feature::server | Feature::privateServer;
inline constexpr Features NETGAME_PRBOOM = Feature::joins;

// The bits a dialect of the ZDoom kind decides one by one.
inline constexpr Features NETGAME_ZDOOM_OWN = Feature::netmode | Feature::teamPick | Feature::netgameFolder;

inline constexpr Features NETGAME_DERIVED = NETGAME_ZDOOM | NETGAME_CHOCOLATE | NETGAME_PRBOOM;

inline constexpr Features COMMON =
        Feature::skill | Feature::noMonsters | Feature::fast | Feature::warp | Feature::pwads | Feature::playdemo;

// No feature changed twice or to the baseline, and every option the port has spelled as its kind needs.
[[nodiscard]] constexpr bool sound(const Port &port) {
    for (std::size_t at = 0; at < port.changes.size(); ++at) {
        const Spelling &each = port.changes[at];

        if (each.arg == dialects::baseline(each.feature)) {
            return false;
        }

        for (std::size_t later = at + 1; later < port.changes.size(); ++later) {
            if (port.changes[later].feature == each.feature) {
                return false;
            }
        }
    }

    return std::ranges::all_of(Options::ALL, [&port](const Options::Option &option) {
        if (!port.has(option.feature)) {
            return true;
        }

        const Arg &spelled = dialects::resolve(port, option.feature);
        const bool spans = spelled.range.least < spelled.range.most;
        const bool none = spelled.range == Range{};

        return !spelled.name.empty() && (option.kind == Options::Kind::number ? spans && spelled.word.empty() : none);
    });
}

// Not constexpr, so a call to it fails the build inside a constant expression and does nothing at run time.
inline void unsound() {
}

// Works out what follows from the values: the netgame features from the kind, recording from the demos,
// and no demo option without demos. A constant that is not sound fails the build.
[[nodiscard]] constexpr Port defined(Port port) {
    Features now = port.features & ~NETGAME_DERIVED;

    switch (port.netgame) {
        case Netgames::zdoom:
            now |= NETGAME_ZDOOM;
            break;
        case Netgames::chocolate:
            now |= NETGAME_CHOCOLATE;
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

    if (now.has(Feature::demos)) {
        now |= Feature::record;
    } else {
        now &= ~(Feature::timedemo | Feature::fastdemo | Feature::continues | Feature::longtics | Feature::soloNet);
    }

    port.features = now;

    if (!sound(port)) {
        unsound();
    }

    return port;
}

[[nodiscard]] constexpr Port atRelease(Port port, const VersionRange &release) {
    port.features = (port.features & ~release.lacks) | release.gains;

    if (release.special != nullptr) {
        release.special(port);
    }

    return port;
}

}

namespace Dialect::dialects {

using enum Feature;

inline constexpr int AND_UP = std::numeric_limits<int>::max();

// NOLINTBEGIN(readability-trailing-comma): a change is one row, with its description inside it
inline constexpr std::array TURBO_TO_400 = std::to_array<Spelling>({
        {turbo,
         {"-turbo", "Player speed in percent, from 10 to 400. 100 is normal.", {}, Range{.least = 10, .most = 400}}},
});
// NOLINTEND(readability-trailing-comma)

// Two change lists as one.
template <std::size_t FIRST, std::size_t SECOND>
constexpr std::array<Spelling, FIRST + SECOND> join(const std::array<Spelling, FIRST> &first,
                                                    const std::array<Spelling, SECOND> &second) {
    std::array<Spelling, FIRST + SECOND> out{};

    for (std::size_t at = 0; at < FIRST; ++at) {
        out[at] = first[at];
    }

    for (std::size_t at = 0; at < SECOND; ++at) {
        out[FIRST + at] = second[at];
    }

    return out;
}

// What Chocolate, Crispy and Woof share.
inline constexpr std::array CHOCOLATE_CHANGES = join(TURBO_TO_400,
                                                     std::to_array<Spelling>({
                                                             {joins, {"-connect"}},
                                                             {timeLimit, {"-timer"}},
                                                     }));

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
