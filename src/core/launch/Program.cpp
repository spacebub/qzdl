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

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "core/launch/Program.h"
#include "core/launch/dialects/Dialects.h"

namespace Program {

namespace {

namespace dialects = Dialect::dialects;

constexpr std::array KNOWN = std::to_array<Known>({
        {
                .program = Program::unknown,
                .port = &dialects::UNKNOWN_PORT,
        },
        {
                .program = Program::doom,
                .words = {"doom", "doom2", "doomu", "doom95", "dosdoom"},
                .match = Match::wholeOutsideFlatpak,
                .port = &dialects::DOOM_PORT,
        },
        {
                .program = Program::legacy,
                .word = "doomlegacy",
                .name = "Doom Legacy",
                .words = {"doom3", "legacy", "doomlegacy"},
                .match = Match::wholeOutsideFlatpak,
                .port = &dialects::LEGACY_PORT,
        },
        {
                .program = Program::helion,
                .word = "helion",
                .name = "Helion",
                .words = {"helion"},
                .port = &dialects::HELION_PORT,
                .saves = {.listing = Listing::helion},
        },
        // Whole name: gzdoom contains zdoom.
        {
                .program = Program::zdoom,
                .word = "zdoom",
                .name = "ZDoom",
                .words = {"zdoom"},
                .match = Match::whole,
                .port = &dialects::ZDOOM_PORT,
                .saves = {.listing = Listing::zdoom, .folder = "ZDoom"},
        },
        {
                .program = Program::zandronum,
                .word = "zandronum",
                .name = "Zandronum",
                .words = {"zandronum", "skulltag"},
                .port = &dialects::ZANDRONUM_PORT,
                .saves = {.listing = Listing::zdoom, .folder = "Zandronum"},
        },
        {
                .program = Program::uzdoom,
                .word = "uzdoom",
                .name = "UZDoom",
                .words = {"uzdoom"},
                .port = &dialects::UZDOOM_PORT,
                .releases = dialects::UZDOOM_RELEASES,
                .saves = {.listing = Listing::gzdoom, .folder = "UZDoom"},
        },
        {
                .program = Program::gzdoom,
                .word = "gzdoom",
                .name = "GZDoom",
                .words = {"gzdoom"},
                .port = &dialects::GZDOOM_PORT,
                .releases = dialects::GZDOOM_RELEASES,
                .saves = {.listing = Listing::gzdoom, .folder = "GZDoom"},
        },
        {
                .program = Program::vkdoom,
                .words = {"vkdoom"},
                .port = &dialects::GZDOOM_PORT,
                .saves = {.listing = Listing::gzdoom, .folder = "VKDoom"},
        },
        {
                .program = Program::zdoomFork,
                .words = {"zdoom"},
                .port = &dialects::GZDOOM_PORT,
        },
        {
                .program = Program::crispyOther,
                .words = {"crispy-heretic", "crispy-hexen", "crispy-strife"},
                .port = &dialects::CRISPY_OTHER,
        },
        {
                .program = Program::crispy,
                .word = "crispy-doom",
                .name = "Crispy Doom",
                .words = {"crispy"},
                .port = &dialects::CRISPY_PORT,
                .saves = {.listing = Listing::chocolate, .folder = "crispy-doom", .slotPrefix = "doomsav"},
        },
        {
                .program = Program::chocolateOther,
                .words = {"chocolate-heretic", "chocolate-hexen", "chocolate-strife"},
                .port = &dialects::VANILLA_OTHER,
        },
        {
                .program = Program::chocolate,
                .word = "chocolate-doom",
                .name = "Chocolate Doom",
                .words = {"chocolate"},
                .port = &dialects::VANILLA_PORT,
                .saves = {.listing = Listing::chocolate, .folder = "chocolate-doom", .slotPrefix = "doomsav"},
        },
        // Whole name: prboom contains boom.
        {
                .program = Program::boom,
                .word = "boom",
                .name = "Boom",
                .words = {"boom"},
                .match = Match::whole,
                .port = &dialects::BOOM_PORT,
        },
        // MBF and WinMBF are the same line, predating -complevel and everything after it.
        {
                .program = Program::mbf,
                .word = "mbf",
                .name = "MBF",
                .words = {"mbf"},
                .port = &dialects::MBF_PORT,
        },
        {
                .program = Program::eternity,
                .words = {"eternity"},
                .port = &dialects::ETERNITY_PORT,
        },
        {
                .program = Program::woof,
                .word = "woof",
                .name = "Woof!",
                .words = {"woof"},
                .port = &dialects::WOOF_PORT,
                .saves = {.listing = Listing::woof, .folder = "woof", .slotPrefix = "woofsav"},
        },
        {
                .program = Program::nugget,
                .word = "nugget-doom",
                .name = "Nugget Doom",
                .words = {"nugget"},
                .port = &dialects::NUGGET_PORT,
                .saves = {.listing = Listing::woof, .folder = "nugget-doom", .slotPrefix = "nuggsav"},
        },
        {
                .program = Program::cherry,
                .words = {"cherry"},
                .port = &dialects::CHERRY_PORT,
        },
        {
                .program = Program::dsda,
                .word = "dsda-doom",
                .name = "DSDA-Doom",
                .words = {"dsda"},
                .port = &dialects::DSDA_PORT,
                .releases = dialects::DSDA_RELEASES,
                .saves = {.listing = Listing::dsda, .folder = "dsda-doom", .slotPrefix = "dsda-doom-savegame"},
        },
        {
                .program = Program::nyan,
                .words = {"nyan"},
                .port = &dialects::NYAN_PORT,
                .saves = {.listing = Listing::dsda, .folder = "nyan-doom", .slotPrefix = "nyan-doom-savegame"},
        },
        {
                .program = Program::prboomPlus,
                .word = "prboom-plus",
                .name = "PrBoom+",
                .words = {"prboom-plus"},
                .port = &dialects::PRBOOM_PORT,
                .saves = {.listing = Listing::prboom, .folder = "prboom-plus", .slotPrefix = "prboom-plus-savegame"},
        },
        {
                .program = Program::glboomPlus,
                .words = {"glboom-plus"},
                .port = &dialects::GLBOOM_PORT,
                .saves = {.listing = Listing::prboom, .folder = "prboom-plus", .slotPrefix = "prboom-plus-savegame"},
        },
        // The original PrBoom, which saves elsewhere.
        {
                .program = Program::prboom,
                .words = {"prboom"},
                .port = &dialects::PRBOOM_PORT,
        },
        {
                .program = Program::glboom,
                .words = {"glboom"},
                .port = &dialects::GLBOOM_PORT,
        },
        {
                .program = Program::rude,
                .words = {"rude"},
                .port = &dialects::RUDE_PORT,
        },
});

static_assert(KNOWN.size() == static_cast<size_t>(Program::rude) + 1, "a row for every program");
static_assert(std::ranges::all_of(KNOWN,
                                  [](const Known &each) { return &KNOWN[static_cast<size_t>(each.program)] == &each; }),
              "KNOWN in program order");

static_assert(std::ranges::all_of(KNOWN,
                                  [](const Known &each) {
                                      return std::ranges::all_of(each.words, &std::string_view::empty)
                                             == (each.program == Program::unknown);
                                  }),
              "every program but unknown has a word");

constexpr bool clash(const Known &one, const Known &other) {
    const bool sameWord = one.match == other.match && std::ranges::any_of(one.words, [&other](const auto word) {
                              return !word.empty() && std::ranges::contains(other.words, word);
                          });

    return sameWord || (!one.word.empty() && one.word == other.word);
}

constexpr bool distinct() {
    for (size_t at = 0; at < KNOWN.size(); ++at) {
        for (size_t later = at + 1; later < KNOWN.size(); ++later) {
            if (clash(KNOWN[at], KNOWN[later])) {
                return false;
            }
        }
    }

    return true;
}

static_assert(distinct(), "no two programs share a port id, or a word under the same rule");

// One bit per character, folded into 64.
constexpr std::uint64_t charactersOf(const std::string_view text) {
    std::uint64_t out = 0;

    for (const char each : text) {
        out |= std::uint64_t{1} << (static_cast<unsigned char>(each) % 64U);
    }

    return out;
}

struct Word {
    // A name without one of these cannot hold the word, so a single test passes over most words.
    std::uint64_t characters;

    std::string_view text;
    const Known *known;
};

constexpr size_t WORD_COUNT = [] {
    size_t out = 0;

    for (const Known &each : KNOWN) {
        out += each.words.size() - static_cast<size_t>(std::ranges::count(each.words, std::string_view()));
    }

    return out;
}();

// Every word in the order a name is matched.
constexpr std::array<Word, WORD_COUNT> WORDS = [] {
    std::array<Word, WORD_COUNT> out{};
    size_t at = 0;

    for (const Known &each : KNOWN) {
        for (const std::string_view word : each.words) {
            if (!word.empty()) {
                out[at++] = {.characters = charactersOf(word), .text = word, .known = &each};
            }
        }
    }

    return out;
}();

bool matches(const Word &word, const std::string_view name, const bool exported) {
    switch (word.known->match) {
        case Match::contains:
            return name.contains(word.text);
        case Match::whole:
            return name == word.text;
        case Match::wholeOutsideFlatpak:
            break;
    }

    return !exported && name == word.text;
}

}

std::span<const Known> known() {
    return KNOWN;
}

const Known &known(const Program program) {
    return KNOWN[static_cast<size_t>(program)];
}

const Known &named(const std::string_view name, const bool exported) {
    const std::uint64_t characters = charactersOf(name);

    for (const Word &each : WORDS) {
        if ((each.characters & ~characters) == 0 && matches(each, name, exported)) {
            return *each.known;
        }
    }

    return KNOWN.front();
}

}
