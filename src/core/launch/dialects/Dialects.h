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

// False for a port that changes a feature twice, repeats the baseline, or spells an option against its kind.
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

// Adds what follows from the kind of netgame, the complevels and the demos. A constant that is not sound
// fails the build.
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

    now = port.complevel == Complevels::none ? now & ~Features(Feature::complevel) : now | Feature::complevel;

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

    if (!release.changes.empty()) {
        port.changes = release.changes;
    }

    if (release.special != nullptr) {
        release.special(port);
    }

    return port;
}

[[nodiscard]] constexpr bool soundAtEvery(const Port &latest, const std::span<const VersionRange> releases) {
    return std::ranges::all_of(releases,
                               [&latest](const VersionRange &each) { return sound(atRelease(latest, each)); });
}

// A row lacks only features the latest release has, and gains only features it lacks.
[[nodiscard]] constexpr bool againstLatest(const Port &latest, const std::span<const VersionRange> releases) {
    return std::ranges::all_of(releases, [&latest](const VersionRange &each) {
        return (each.lacks & ~latest.features) == Features() && (each.gains & latest.features) == Features();
    });
}

// Whether a release such as 10.2.0 comes after another, compared number by number.
[[nodiscard]] constexpr bool later(std::string_view release, std::string_view other) {
    const auto next = [](std::string_view &rest) {
        int number = 0;

        while (!rest.empty() && rest.front() != '.') {
            number = (number * 10) + (rest.front() - '0');
            rest.remove_prefix(1);
        }

        if (!rest.empty()) {
            rest.remove_prefix(1);
        }

        return number;
    };

    while (!release.empty() || !other.empty()) {
        const int mine = next(release);
        const int theirs = next(other);

        if (mine != theirs) {
            return mine > theirs;
        }
    }

    return false;
}

[[nodiscard]] constexpr bool newestFirst(const std::span<const VersionRange> releases) {
    return std::ranges::adjacent_find(releases,
                                      [](const VersionRange &newer, const VersionRange &older) {
                                          return !later(newer.first, older.first);
                                      })
           == releases.end();
}

// A release table, checked as it is made. A table that is not newest first, not stated against the latest or not
// sound at every row fails the build.
template <std::size_t COUNT>
struct Releases {
    std::array<VersionRange, COUNT> rows;

    constexpr Releases(const Port &latest, const std::array<VersionRange, COUNT> &table) : rows(table) {
        if (!newestFirst(rows) || !soundAtEvery(latest, rows) || !againstLatest(latest, rows)) {
            unsound();
        }
    }
};

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

[[nodiscard]] constexpr Port withConfig(Port port, const std::string_view configName) {
    port.configName = configName;

    return port;
}

// What the programs speak. The table in Program.cpp says which program speaks which.
extern const Port GZDOOM_PORT;
extern const Port UZDOOM_PORT;
extern const Port ZDOOM_PORT;
extern const Port ZANDRONUM_PORT;
extern const Port UNKNOWN_PORT;
extern const Port DSDA_PORT;
extern const Port NYAN_PORT;
extern const Port PRBOOM_PORT;
extern const Port GLBOOM_PORT;
extern const Port RUDE_PORT;
extern const Port ETERNITY_PORT;
extern const Port WOOF_PORT;
extern const Port NUGGET_PORT;
extern const Port CHERRY_PORT;
extern const Port CHOCOLATE_PORT;
extern const Port CHOCOLATE_OTHER;
extern const Port CRISPY_PORT;
extern const Port CRISPY_OTHER;
extern const Port HELION_PORT;
extern const Port BOOM_PORT;
extern const Port MBF_PORT;
extern const Port LEGACY_PORT;
extern const Port DOOM_PORT;

extern const Releases<6> GZDOOM_RELEASES;
extern const Releases<2> UZDOOM_RELEASES;
extern const Releases<8> DSDA_RELEASES;
extern const Releases<7> WOOF_RELEASES;
extern const Releases<7> NUGGET_RELEASES;
extern const Releases<2> CHERRY_RELEASES;

// The -complevel numbers a kind of port accepts.
struct Numbering {
    // -1 first, the port's own.
    std::span<const int> levels;

    // The level as it goes on the command line. Null for the number.
    std::string_view (*levelWord)(int level);
};

extern const Numbering PRBOOM_NUMBERING;
extern const Numbering DSDA_NUMBERING;
extern const Numbering WOOF_NUMBERING;
extern const Numbering WOOF_TO_MBF21_NUMBERING;
extern const Numbering HELION_NUMBERING;
extern const Numbering GAME_VERSION_NUMBERING;

// What a game version is called, such as Ultimate Doom. Empty for any other level.
[[nodiscard]] std::string_view gameVersionName(int level);

}
