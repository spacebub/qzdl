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

#include "core/config/Schema.h"

namespace Enums {

// Every enumerator up to `LAST`, in order.
template <typename Enum, Enum LAST>
constexpr std::array<Enum, static_cast<std::size_t>(LAST) + 1> every() {
    std::array<Enum, static_cast<std::size_t>(LAST) + 1> out{};

    for (std::size_t at = 0; at < out.size(); ++at) {
        out[at] = static_cast<Enum>(at);
    }

    return out;
}

}

namespace Flags {

// In the order the fields and the dialog footers list them.
enum class Word : std::uint8_t {
    dmflags,
    dmflags2,
    dmflags3,
    zadmflags,
    lmsWeapons,
    lmsSpectators,
    compatflags,
    compatflags2,
    zacompatflags,
};

inline constexpr std::array ALL = Enums::every<Word, Word::zacompatflags>();
inline constexpr std::size_t WORDS = ALL.size();

[[nodiscard]] constexpr std::size_t index(const Word word) {
    return static_cast<std::size_t>(word);
}

// The cvar, and the key the profile keeps the word under.
[[nodiscard]] constexpr const char *name(const Word word) {
    switch (word) {
        case Word::dmflags:
            return FlagWord::DMFLAGS;
        case Word::dmflags2:
            return FlagWord::DMFLAGS2;
        case Word::dmflags3:
            return FlagWord::DMFLAGS3;
        case Word::zadmflags:
            return FlagWord::ZADMFLAGS;
        case Word::lmsWeapons:
            return FlagWord::LMS_WEAPONS;
        case Word::lmsSpectators:
            return FlagWord::LMS_SPECTATORS;
        case Word::compatflags:
            return FlagWord::COMPATFLAGS;
        case Word::compatflags2:
            return FlagWord::COMPATFLAGS2;
        case Word::zacompatflags:
            return FlagWord::ZACOMPATFLAGS;
    }

    return "";
}

}

namespace Rules {

// Switches that change play beyond the flag words, in the order the Gameplay section shows them: the ones that
// turn on, then the ones that take a number.
enum class Rule : std::uint8_t {
    coopSpawns,
    coopPreset,
    alwaysApplyDmflags,
    chainEpisodes,
    doubleAmmo,
    noCheats,
    noOptions,
    limitRemoving,
    vanilla,
    beta,
    stroller,
    flipLevels,
    flipWeapons,
    turbo,
    speed,
    dogs,
};

inline constexpr std::array ALL = Enums::every<Rule, Rule::dogs>();
inline constexpr std::size_t RULES = ALL.size();

[[nodiscard]] constexpr bool takesNumber(const Rule rule) {
    return rule >= Rule::turbo;
}

[[nodiscard]] constexpr std::size_t index(const Rule rule) {
    return static_cast<std::size_t>(rule);
}

// The key the profile keeps the rule under.
[[nodiscard]] constexpr const char *key(const Rule rule) {
    switch (rule) {
        case Rule::coopSpawns:
            return RuleKey::COOP_SPAWNS;
        case Rule::coopPreset:
            return RuleKey::COOP;
        case Rule::alwaysApplyDmflags:
            return RuleKey::ALWAYS_APPLY_DMFLAGS;
        case Rule::chainEpisodes:
            return RuleKey::CHAIN_EPISODES;
        case Rule::doubleAmmo:
            return RuleKey::DOUBLE_AMMO;
        case Rule::noCheats:
            return RuleKey::NO_CHEATS;
        case Rule::noOptions:
            return RuleKey::NO_OPTIONS;
        case Rule::limitRemoving:
            return RuleKey::LIMIT_REMOVING;
        case Rule::vanilla:
            return RuleKey::VANILLA;
        case Rule::beta:
            return RuleKey::BETA;
        case Rule::stroller:
            return RuleKey::STROLLER;
        case Rule::flipLevels:
            return RuleKey::FLIP_LEVELS;
        case Rule::flipWeapons:
            return RuleKey::FLIP_WEAPONS;
        case Rule::turbo:
            return RuleKey::TURBO;
        case Rule::speed:
            return RuleKey::SPEED;
        case Rule::dogs:
            return RuleKey::DOGS;
    }

    return "";
}

}
