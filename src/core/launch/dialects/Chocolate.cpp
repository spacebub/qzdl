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
#include <cstddef>
#include <utility>

#include "core/launch/dialects/Dialects.h"

namespace Dialect::dialects {

namespace {

struct GameVersion {
    std::string_view word;
    std::string_view name;
};

// Stored by place from GAME_VERSIONS on, so new ones go at the end.
constexpr std::array VERSIONS = std::to_array<GameVersion>({
        {"1.2", "Doom 1.2"},
        {"1.5", "Doom 1.5"},
        {"1.666", "Doom 1.666"},
        {"1.7", "Doom 1.7"},
        {"1.8", "Doom 1.8"},
        {"1.9", "Doom 1.9"},
        {"ultimate", "Ultimate Doom"},
        {"final", "Final Doom"},
        {"final2", "Final Doom, other release"},
        {"hacx", "Hacx"},
        {"chex", "Chex Quest"},
});

// -1 first, the port's own.
constexpr std::array<int, VERSIONS.size() + 1> VERSION_LEVELS = [] {
    std::array<int, VERSIONS.size() + 1> out{-1};

    for (size_t at = 0; at < VERSIONS.size(); ++at) {
        out[at + 1] = GAME_VERSIONS + static_cast<int>(at);
    }

    return out;
}();

const GameVersion *versionAt(const int level) {
    const int at = level - GAME_VERSIONS;

    return at >= 0 && std::cmp_less(at, VERSIONS.size()) ? &VERSIONS[static_cast<size_t>(at)] : nullptr;
}

std::string_view gameVersionWord(const int level) {
    const GameVersion *version = versionAt(level);

    return version == nullptr ? std::string_view() : version->word;
}

constexpr std::array CRISPY_CHANGES = join(CHOCOLATE_CHANGES, std::to_array<Spelling>({{complevel, {"-gameversion"}}}));

// Chocolate Doom finds the sprites and flats of a PWAD only when it is merged in.
constexpr std::array VANILLA_CHANGES = join(CRISPY_CHANGES, std::to_array<Spelling>({{pwads, {"-merge"}}}));

// Chocolate plus -levelstat, which it has written since 5.9.0. It merges every -file on its own.
constexpr Traits crispy(Traits traits) {
    traits.features |= levelstat | pistolStart | coopSpawns | doubleAmmo | flipLevels | flipWeapons | deathmatch3;
    traits.configName = "crispy";
    traits.changes = CRISPY_CHANGES;
    traits.line = PortLine::crispy;

    return defined(traits);
}

// Heretic, Hexen and Strife come in the same packages, with game versions and switches of their own.
constexpr Traits otherGame(Traits traits) {
    traits.complevel = Complevels::none;
    traits.features &= ~Features(deathmatch3);
    traits.line = PortLine::none;

    // Of the rules, they read only pistol start.
    traits.features &= ~Options::RULES | pistolStart;

    return defined(traits);
}

}

constexpr Traits CHOCOLATE_TRAITS = defined({
        .features = COMMON | recognised | iwad | extraConfig | configFile | saveFolder | deh | bex | noAutoload | nosfx
                    | fullscreen | windowed | respawn | loadgame | demos | timedemo | longtics | soloNet | recordAddsLmp
                    | nomusic | displayKept | noCheats | turbo,
        .saveExt = ".dsg",
        .configName = "chocolate",
        .changes = VANILLA_CHANGES,
        .dialect = Dialect::chocolate,
        .loads = SaveNames::slot,
        .netgame = Netgames::chocolate,
        .complevel = Complevels::gameVersion,
        .line = PortLine::chocolate,
});

constexpr Traits CRISPY_TRAITS = crispy(CHOCOLATE_TRAITS);
constexpr Traits CRISPY_OTHER_TRAITS = otherGame(CRISPY_TRAITS);
constexpr Traits CHOCOLATE_OTHER_TRAITS = otherGame(CHOCOLATE_TRAITS);

std::string_view gameVersionName(const int level) {
    const GameVersion *version = versionAt(level);

    return version == nullptr ? std::string_view() : version->name;
}

constexpr Numbering GAME_VERSION_NUMBERING{.levels = VERSION_LEVELS, .levelWord = gameVersionWord};

}
