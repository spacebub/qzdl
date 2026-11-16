/*
 * This file is part of qZDL
 * Copyright (C) 2026  spacebub
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

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "core/config/Schema.h"
#include "core/launch/Feature.h"

namespace Options {

enum class Kind : std::uint8_t {
    toggle,
    number,
};

struct Option {
    Dialect::Feature feature;

    // The key under rules in a profile and in a .zdl. Null for an option the files keep elsewhere.
    const char *key;

    std::string_view label;

    // Shown in an empty number field. Empty for the default text.
    std::string_view placeholder;

    Kind kind;
};

// In the order the Gameplay section shows them and the command line takes them.
inline constexpr std::array ALL = [] {
    using enum Dialect::Feature;
    using enum Kind;

    // clang-format off
    return std::to_array<Option>({
            {pistolStart,        nullptr,                       "Pistol start",                {},    toggle},
            {cheats,             nullptr,                       "Allow cheats",                {},    toggle},
            {coopSpawns,         RuleKey::COOP_SPAWNS,          "Coop things",                 {},    toggle},
            {coopPreset,         RuleKey::COOP,                 "Coop preset",                 {},    toggle},
            {alwaysApplyDmflags, RuleKey::ALWAYS_APPLY_DMFLAGS, "Deathmatch flags everywhere", {},    toggle},
            {chainEpisodes,      RuleKey::CHAIN_EPISODES,       "Chain episodes",              {},    toggle},
            {doubleAmmo,         RuleKey::DOUBLE_AMMO,          "Double ammo",                 {},    toggle},
            {noCheats,           RuleKey::NO_CHEATS,            "Ignore patch cheats",         {},    toggle},
            {noOptions,          RuleKey::NO_OPTIONS,           "Skip OPTIONS lumps",          {},    toggle},
            {limitRemoving,      RuleKey::LIMIT_REMOVING,       "Limit removing",              {},    toggle},
            {vanillaMode,        RuleKey::VANILLA,              "Vanilla mode",                {},    toggle},
            {mbfBeta,            RuleKey::BETA,                 "MBF beta",                    {},    toggle},
            {stroller,           RuleKey::STROLLER,             "Stroller",                    {},    toggle},
            {flipLevels,         RuleKey::FLIP_LEVELS,          "Mirrored maps",               {},    toggle},
            {flipWeapons,        RuleKey::FLIP_WEAPONS,         "Mirrored weapons",            {},    toggle},
            {turbo,              RuleKey::TURBO,                "Turbo",                       "100", number},
            {gameSpeed,          RuleKey::SPEED,                "Game speed",                  "100", number},
            {dogs,               RuleKey::DOGS,                 "Helper dogs",                 {},    number},
    });
    // clang-format on
}();

// The row of the option, or ALL.size() when the feature is not an option.
[[nodiscard]] constexpr std::size_t indexOf(const Dialect::Feature feature) {
    std::size_t at = 0;

    while (at < ALL.size() && ALL[at].feature != feature) {
        ++at;
    }

    return at;
}

// Every feature the user can set.
inline constexpr Dialect::Features FEATURES = [] {
    Dialect::Features out;

    for (const Option &each : ALL) {
        out |= each.feature;
    }

    return out;
}();

}
