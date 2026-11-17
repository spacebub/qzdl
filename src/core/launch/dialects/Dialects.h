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
inline constexpr Features NETGAME_ZDOOM = NETGAME_HOST | Feature::joins | Feature::hostPlayers | Feature::fragLimit
                                          | Feature::savegame | Feature::extratic | Feature::dup | Feature::player
                                          | Feature::teams;
inline constexpr Features NETGAME_CHOCOLATE =
        NETGAME_HOST | Feature::joins | Feature::dup | Feature::hostListed | Feature::hostUnlisted;
inline constexpr Features NETGAME_PRBOOM = Feature::joins;

// The bits a dialect of the ZDoom kind decides one by one.
inline constexpr Features NETGAME_ZDOOM_OWN =
        Feature::netmode | Feature::teamPick | Feature::netgameFolder | Feature::hostsServer;

// A server and its clients never form a second node, so there are no tics to tune.
inline constexpr Features NODE_TUNING = Feature::netmode | Feature::dup | Feature::extratic;

inline constexpr Features NETGAME_DERIVED = NETGAME_ZDOOM | NETGAME_CHOCOLATE | NETGAME_PRBOOM;

inline constexpr Features COMMON =
        Feature::skill | Feature::noMonsters | Feature::fast | Feature::warp | Feature::pwads | Feature::playdemo;

// False for traits that change a feature twice, repeat the baseline, or spell an option against its kind.
[[nodiscard]] constexpr bool sound(const Traits &traits) {
    for (std::size_t at = 0; at < traits.changes.size(); ++at) {
        const Spelling &each = traits.changes[at];

        if (each.arg == dialects::baseline(each.feature)) {
            return false;
        }

        for (std::size_t later = at + 1; later < traits.changes.size(); ++later) {
            if (traits.changes[later].feature == each.feature) {
                return false;
            }
        }
    }

    return std::ranges::all_of(Options::ALL, [&traits](const Options::Option &option) {
        if (!traits.has(option.feature)) {
            return true;
        }

        const Arg &spelled = dialects::resolve(traits, option.feature);
        const bool spans = spelled.range.least < spelled.range.most;
        const bool none = spelled.range == Range{};

        return !spelled.name.empty() && (option.kind == Options::Kind::number ? spans && spelled.word.empty() : none);
    });
}

// Not constexpr, so a call to it fails the build inside a constant expression and does nothing at run time.
inline void unsound() {
}

// Adds what follows from the kind of netgame, the complevels and the replays. A constant that is not sound
// fails the build.
[[nodiscard]] constexpr Traits defined(Traits traits) {
    Features now = traits.features & ~NETGAME_DERIVED;

    switch (traits.netgame) {
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

    if (traits.netgame != Netgames::zdoom) {
        now &= ~NETGAME_ZDOOM_OWN;
    }

    if (now.has(Feature::hostsServer)) {
        now &= ~NODE_TUNING;
    }

    now = traits.complevel == Complevels::none ? now & ~Features(Feature::complevel) : now | Feature::complevel;

    if (now.has(Feature::replays)) {
        now |= Feature::record;
    } else {
        now &= ~(Feature::timedemo | Feature::fastdemo | Feature::continues | Feature::longtics | Feature::soloNet);
    }

    traits.features = now;

    if (!sound(traits)) {
        unsound();
    }

    return traits;
}

[[nodiscard]] constexpr Traits atRelease(Traits traits, const VersionRange &release) {
    traits.features = (traits.features & ~release.lacks) | release.gains;

    if (!release.changes.empty()) {
        traits.changes = release.changes;
    }

    if (release.special != nullptr) {
        release.special(traits);
    }

    return traits;
}

[[nodiscard]] constexpr bool soundAtEvery(const Traits &latest, const std::span<const VersionRange> releases) {
    return std::ranges::all_of(releases,
                               [&latest](const VersionRange &each) { return sound(atRelease(latest, each)); });
}

// A row lacks only features the latest release has, and gains only features it lacks.
[[nodiscard]] constexpr bool againstLatest(const Traits &latest, const std::span<const VersionRange> releases) {
    return std::ranges::all_of(releases, [&latest](const VersionRange &each) {
        return (each.lacks & ~latest.features) == Features() && (each.gains & latest.features) == Features();
    });
}

// A run of digits stops growing here, so a long one cannot overflow.
inline constexpr int RUN_CAP = 99'999'999;

// Whether a release such as 10.2.0 or 0.97d2 comes after another. Each run of digits counts as a number, and each
// letter as one more part, whatever its case.
[[nodiscard]] constexpr bool later(std::string_view release, std::string_view other) {
    const auto digit = [](const char each) { return each >= '0' && each <= '9'; };
    const auto next = [&digit](std::string_view &rest) {
        if (!rest.empty() && rest.front() == '.') {
            rest.remove_prefix(1);
        }

        if (!rest.empty() && !digit(rest.front())) {
            const char letter = rest.front();
            rest.remove_prefix(1);

            return static_cast<int>(
                    static_cast<unsigned char>(letter >= 'A' && letter <= 'Z' ? letter - 'A' + 'a' : letter));
        }

        int number = 0;

        while (!rest.empty() && digit(rest.front())) {
            number = std::min(RUN_CAP, (number * 10) + (rest.front() - '0'));
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

    constexpr Releases(const Traits &latest, const std::array<VersionRange, COUNT> &table) : rows(table) {
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
inline constexpr std::array CHOCOLATE_LINE_CHANGES = join(TURBO_TO_400,
                                                          std::to_array<Spelling>({
                                                                  {joins, {"-connect"}},
                                                                  {timeLimit, {"-timer"}},
                                                          }));

[[nodiscard]] constexpr Traits withConfig(Traits traits, const std::string_view configName) {
    traits.configName = configName;

    return traits;
}

// What the ports speak. The table in KnownPorts.cpp says which port speaks which.
extern const Traits GZDOOM_TRAITS;
extern const Traits UZDOOM_TRAITS;
extern const Traits ZDOOM_TRAITS;
extern const Traits ZANDRONUM_TRAITS;
extern const Traits SKULLTAG_TRAITS;
extern const Traits UNKNOWN_TRAITS;
extern const Traits DSDA_TRAITS;
extern const Traits NYAN_TRAITS;
extern const Traits PRBOOM_TRAITS;
extern const Traits GLBOOM_TRAITS;
extern const Traits RUDE_TRAITS;
extern const Traits ETERNITY_TRAITS;
extern const Traits WOOF_TRAITS;
extern const Traits NUGGET_TRAITS;
extern const Traits CHERRY_TRAITS;
extern const Traits CHOCOLATE_TRAITS;
extern const Traits CHOCOLATE_OTHER_TRAITS;
extern const Traits CRISPY_TRAITS;
extern const Traits CRISPY_OTHER_TRAITS;
extern const Traits HELION_TRAITS;
extern const Traits BOOM_TRAITS;
extern const Traits MBF_TRAITS;
extern const Traits LEGACY_TRAITS;
extern const Traits DOOM_TRAITS;

extern const Releases<6> GZDOOM_RELEASES;
extern const Releases<2> UZDOOM_RELEASES;
extern const Releases<2> VKDOOM_RELEASES;
extern const Releases<3> SKULLTAG_RELEASES;
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
