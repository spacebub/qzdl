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
#include <utility>
#include <vector>

#include "core/config/GameplayKeys.h"
#include "core/launch/Feature.h"
#include "core/launch/PortLine.h"

struct Profile;

namespace Flags {

// Which dialog a word is edited in.
enum class Family : std::uint8_t {
    gameplay,
    compatibility,
};

// The words a dialog changed, each as it goes back into its field. Empty leaves the word to the port.
using Edits = std::vector<std::pair<Word, std::string>>;

// Its cvar is Flags::name().
struct WordInfo {
    Word word;

    // What the port runs with when the word is not given.
    std::uint32_t start;

    Family family;
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

// A feature the port must have, or must lack, for an option to be offered. count asks for none.
struct Gate {
    Dialect::Feature feature{Dialect::Feature::count};
    bool lacking{false};
};

// One check box, or one choice between the values a few bits make together.
struct Option {
    Word word;
    std::uint32_t mask;
    PortLines lines;
    Group group;
    std::string_view label;

    // The cvar for the bit alone. Empty for a choice and for a bit only a map can set.
    std::string_view cvar;

    std::string_view description;

    // Empty for a check box.
    std::span<const Choice> choices{};

    // For a bit whose meaning changed between releases of one line.
    Gate gate{};
};

// A compatibility mode of a ZDoom family port, as its menu offers it.
struct Mode {
    std::string_view label;
    std::uint32_t compatflags;
    std::uint32_t compatflags2;
};

[[nodiscard]] const WordInfo &info(Word word);

[[nodiscard]] std::string_view title(Group group);

// Every option, grouped in dialog order.
[[nodiscard]] std::span<const Option> options();

// Whether a port on the line with these features is offered the option.
[[nodiscard]] bool offered(const Option &option, PortLine line, const Dialect::Features &features);

// The bits a line gives a meaning to in the word.
[[nodiscard]] std::uint32_t known(PortLine line, Word word);

// In the port menu order. Empty for a line without them.
[[nodiscard]] std::span<const Mode> modes(PortLine line);

// The words the modes of a line fill. A mode leaves any other word as it is.
[[nodiscard]] std::span<const Word> modeWords(PortLine line);

// What the level list of the line says its modes fill. Empty for a line without modes.
[[nodiscard]] std::string_view modeHint(PortLine line);

// The mode the words of the profile match: 0 for the port's own, the mode after it, or -1 when they match none.
[[nodiscard]] int modeIndex(PortLine line, const Profile &profile);

// Fills the words of the mode, counted as modeIndex counts. 0 empties them.
void setMode(PortLine line, Profile &profile, int index);

// A whole decimal number that fits the 32 bits of a word. Empty text is not one.
[[nodiscard]] std::optional<std::uint32_t> parse(std::string_view typed);

// The value in decimal, as the port reads it back whole.
[[nodiscard]] std::string decimal(std::uint32_t value, bool signedWord);

}
