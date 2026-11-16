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

// Where the files keep an option.
enum class Place : std::uint8_t {
    // A bool of the profile itself.
    profile,

    // Text under rules.
    rules,
};

// Which part of the profile page shows an option.
enum class Group : std::uint8_t {
    files,
    sound,

    // The rules of the game, which a joining player takes from the host.
    rules,
};

struct Option {
    Dialect::Feature feature;

    // The key in zdl.json, then the key in a .zdl.
    const char *key;
    const char *iniKey;

    std::string_view label;

    // Shown in an empty number field. Empty for the default text.
    std::string_view placeholder;

    Kind kind;
    Place place;
    Group group;

    // The toggle reads on while the switch stays off the command line, as Music does for -nomusic.
    bool inverted;
};

// In the order the panels show them and the command line takes them.
inline constexpr std::array ALL = [] {
    using enum Dialect::Feature;
    using enum Group;
    using enum Kind;

    constexpr bool INVERTED = true;

    // clang-format off
    const auto onProfile = [](const Dialect::Feature feature, const char *key, const char *iniKey,
                              const std::string_view label, const Group group, const bool inverted = false) {
        return Option{feature, key, iniKey, label, {}, toggle, Place::profile, group, inverted};
    };

    // Both files keep a rule under the same key.
    const auto rule = [](const Dialect::Feature feature, const char *key, const std::string_view label,
                         const Kind kind = toggle, const std::string_view placeholder = {}) {
        return Option{feature, key, key, label, placeholder, kind, Place::rules, Group::rules, false};
    };

    return std::to_array<Option>({
            onProfile(levelstat,   ProfileKey::LEVELSTAT,     IniKey::LEVELSTAT,     "Level stats",   files),
            onProfile(noAutoload,  ProfileKey::SKIP_AUTOLOAD, IniKey::NO_AUTOLOAD,   "Skip autoload", files),
            onProfile(nomusic,     ProfileKey::MUSIC,         IniKey::MUSIC,         "Music",         sound, INVERTED),
            onProfile(nosfx,       ProfileKey::SOUND_EFFECTS, IniKey::SOUND_EFFECTS, "Sound effects", sound, INVERTED),
            onProfile(pistolStart, ProfileKey::PISTOL_START,  IniKey::PISTOL_START,  "Pistol start",  rules),
            onProfile(cheats,      ProfileKey::CHEATS,        IniKey::CHEATS,        "Allow cheats",  rules),
            rule(coopSpawns,         RuleKey::COOP_SPAWNS,          "Coop things"),
            rule(coopPreset,         RuleKey::COOP,                 "Coop preset"),
            rule(alwaysApplyDmflags, RuleKey::ALWAYS_APPLY_DMFLAGS, "Deathmatch flags everywhere"),
            rule(chainEpisodes,      RuleKey::CHAIN_EPISODES,       "Chain episodes"),
            rule(doubleAmmo,         RuleKey::DOUBLE_AMMO,          "Double ammo"),
            rule(noCheats,           RuleKey::NO_CHEATS,            "Ignore patch cheats"),
            rule(noOptions,          RuleKey::NO_OPTIONS,           "Skip OPTIONS lumps"),
            rule(limitRemoving,      RuleKey::LIMIT_REMOVING,       "Limit removing"),
            rule(vanillaMode,        RuleKey::VANILLA,              "Vanilla mode"),
            rule(mbfBeta,            RuleKey::BETA,                 "MBF beta"),
            rule(stroller,           RuleKey::STROLLER,             "Stroller"),
            rule(flipLevels,         RuleKey::FLIP_LEVELS,          "Mirrored maps"),
            rule(flipWeapons,        RuleKey::FLIP_WEAPONS,         "Mirrored weapons"),
            rule(turbo,              RuleKey::TURBO,                "Turbo",       number, "100"),
            rule(gameSpeed,          RuleKey::SPEED,                "Game speed",  number, "100"),
            rule(dogs,               RuleKey::DOGS,                 "Helper dogs", number),
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

// Every rule of the game the user can set.
inline constexpr Dialect::Features RULES = [] {
    Dialect::Features out;

    for (const Option &each : ALL) {
        if (each.group == Group::rules) {
            out |= each.feature;
        }
    }

    return out;
}();

}
