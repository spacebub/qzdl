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

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace Flags {

// The ports that read flag words. The same bit can mean something else on each.
enum class Set : std::uint8_t {
    none,
    uzdoom,
    gzdoom,
    zdoom,
    zandronum,
    eternity,
};

// Several sets at once, one bit each.
using Sets = std::uint32_t;

[[nodiscard]] constexpr Sets bit(const Set set) {
    return set == Set::none ? 0U : 1U << (static_cast<unsigned>(set) - 1U);
}

// Which dialog a word is edited in.
enum class Family : std::uint8_t {
    gameplay,
    compatibility,
};

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

inline constexpr std::size_t WORDS = static_cast<std::size_t>(Word::zacompatflags) + 1;

struct WordInfo {
    // The cvar, and the key the profile keeps it under.
    std::string_view name;
    Family family;
    Sets sets;

    // What the port runs with when the word is not given.
    std::uint32_t start;

    // The ports that write the word to their own settings, where it outlives the launch.
    Sets kept;
};

// In the order the dialog shows them.
enum class Group : std::uint8_t {
    monsters,
    items,
    weapons,
    players,
    respawn,
    maps,
    cooperative,
    deathmatch,
    lastManStanding,
    lastManWeapons,
    actors,
    dehacked,
    mapActions,
    physics,
    rendering,
    sound,
    scripts,
    online,
};

inline constexpr std::size_t GROUPS = static_cast<std::size_t>(Group::online) + 1;

struct Choice {
    std::uint32_t value;
    std::string_view label;
};

// One check box, or one choice between the values a few bits make together.
struct Option {
    Word word;
    std::uint32_t mask;
    Sets sets;
    Group group;
    std::string_view label;

    // The cvar for the bit alone. Empty for a choice and for a bit only a map can set.
    std::string_view cvar;

    std::string_view description;

    // Empty for a check box.
    std::span<const Choice> choices{};
};

// A compatibility mode of a ZDoom family port, as its menu offers it.
struct Preset {
    std::string_view label;
    std::uint32_t compatflags;
    std::uint32_t compatflags2;
};

[[nodiscard]] const WordInfo &info(Word word);

[[nodiscard]] std::string_view title(Group group);

[[nodiscard]] bool reads(Set set, Word word);

// Every option, grouped in dialog order.
[[nodiscard]] std::span<const Option> options();

// The bits a set gives a meaning to in the word.
[[nodiscard]] std::uint32_t known(Set set, Word word);

// In the port menu order. Empty for a set without them.
[[nodiscard]] std::span<const Preset> presets(Set set);

// A whole decimal number that fits the 32 bits of a word. Empty text is not one.
[[nodiscard]] std::optional<std::uint32_t> parse(std::string_view typed);

// The switch that sets the word on the command line, such as +dmflags.
[[nodiscard]] std::string switchFor(Set set, Word word);

// The value as the port reads it back whole, in decimal.
[[nodiscard]] std::string spelled(Set set, std::uint32_t value);

}
