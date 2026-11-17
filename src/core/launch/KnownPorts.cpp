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
#include <span>
#include <string>
#include <string_view>

#include "ttk/system/Text.h"

#include "core/launch/KnownPorts.h"
#include "core/launch/dialects/Dialects.h"
#include "core/ports/Detect.h"

using namespace ttk;

namespace Port {

namespace {

namespace dialects = Dialect::dialects;

constexpr std::array KNOWN = std::to_array<Known>({
        {
                .port = Port::unknown,
                .traits = &dialects::UNKNOWN_TRAITS,
        },
        {
                .port = Port::doom,
                .id = "doom",
                .name = "Vanilla Doom",
                .words = {"doom", "doom2", "doomu", "doom95", "dosdoom"},
                .match = Match::wholeOutsideFlatpak,
                .traits = &dialects::DOOM_TRAITS,
        },
        {
                .port = Port::legacy,
                .id = "doomlegacy",
                .name = "Doom Legacy",
                .words = {"doom3", "legacy", "doomlegacy"},
                .match = Match::wholeOutsideFlatpak,
                .traits = &dialects::LEGACY_TRAITS,
        },
        {
                .port = Port::helion,
                .id = "helion",
                .name = "Helion",
                .words = {"helion"},
                .traits = &dialects::HELION_TRAITS,
                .dataName = "Helion",
        },
        // Whole name: gzdoom contains zdoom.
        {
                .port = Port::zdoom,
                .id = "zdoom",
                .name = "ZDoom",
                .words = {"zdoom"},
                .match = Match::whole,
                .traits = &dialects::ZDOOM_TRAITS,
                .dataName = "ZDoom",
        },
        {
                .port = Port::zandronum,
                .id = "zandronum",
                .name = "Zandronum",
                .words = {"zandronum"},
                .traits = &dialects::ZANDRONUM_TRAITS,
                .dataName = "Zandronum",
        },
        {
                .port = Port::skulltag,
                .id = "skulltag",
                .name = "Skulltag",
                .words = {"skulltag"},
                .traits = &dialects::SKULLTAG_TRAITS,
                .releases = dialects::SKULLTAG_RELEASES.rows,
                .dataName = "Skulltag",
        },
        {
                .port = Port::uzdoom,
                .id = "uzdoom",
                .name = "UZDoom",
                .words = {"uzdoom"},
                .traits = &dialects::UZDOOM_TRAITS,
                .releases = dialects::UZDOOM_RELEASES.rows,
                .dataName = "UZDoom",
        },
        {
                .port = Port::gzdoom,
                .id = "gzdoom",
                .name = "GZDoom",
                .words = {"gzdoom"},
                .traits = &dialects::GZDOOM_TRAITS,
                .releases = dialects::GZDOOM_RELEASES.rows,
                .dataName = "GZDoom",
        },
        {
                .port = Port::vkdoom,
                .id = "vkdoom",
                .name = "VKDoom",
                .words = {"vkdoom"},
                .traits = &dialects::GZDOOM_TRAITS,
                .releases = dialects::VKDOOM_RELEASES.rows,
                .dataName = "VKDoom",
        },
        {
                .port = Port::zdoomFork,
                .words = {"zdoom"},
                .traits = &dialects::GZDOOM_TRAITS,
        },
        {
                .port = Port::crispyOther,
                .words = {"crispy-heretic", "crispy-hexen", "crispy-strife"},
                .traits = &dialects::CRISPY_OTHER_TRAITS,
        },
        {
                .port = Port::crispy,
                .id = "crispy-doom",
                .name = "Crispy Doom",
                .words = {"crispy"},
                .traits = &dialects::CRISPY_TRAITS,
                .dataName = "crispy-doom",
                .slotPrefix = "doomsav",
        },
        {
                .port = Port::chocolateOther,
                .words = {"chocolate-heretic", "chocolate-hexen", "chocolate-strife"},
                .traits = &dialects::CHOCOLATE_OTHER_TRAITS,
        },
        {
                .port = Port::chocolate,
                .id = "chocolate-doom",
                .name = "Chocolate Doom",
                .words = {"chocolate"},
                .traits = &dialects::CHOCOLATE_TRAITS,
                .dataName = "chocolate-doom",
                .slotPrefix = "doomsav",
        },
        // Whole name: prboom contains boom.
        {
                .port = Port::boom,
                .id = "boom",
                .name = "Boom",
                .words = {"boom"},
                .match = Match::whole,
                .traits = &dialects::BOOM_TRAITS,
        },
        // MBF and WinMBF are the same line, predating -complevel and everything after it.
        {
                .port = Port::mbf,
                .id = "mbf",
                .name = "MBF",
                .words = {"mbf"},
                .traits = &dialects::MBF_TRAITS,
        },
        {
                .port = Port::eternity,
                .id = "eternity",
                .name = "Eternity Engine",
                .words = {"eternity"},
                .traits = &dialects::ETERNITY_TRAITS,
        },
        {
                .port = Port::woof,
                .id = "woof",
                .name = "Woof!",
                .words = {"woof"},
                .traits = &dialects::WOOF_TRAITS,
                .releases = dialects::WOOF_RELEASES.rows,
                .dataName = "woof",
                .slotPrefix = "woofsav",
        },
        {
                .port = Port::nugget,
                .id = "nugget-doom",
                .name = "Nugget Doom",
                .words = {"nugget"},
                .traits = &dialects::NUGGET_TRAITS,
                .releases = dialects::NUGGET_RELEASES.rows,
                .dataName = "nugget-doom",
                .slotPrefix = "nuggsav",
        },
        {
                .port = Port::cherry,
                .id = "cherry-doom",
                .name = "Cherry Doom",
                .words = {"cherry"},
                .traits = &dialects::CHERRY_TRAITS,
                .releases = dialects::CHERRY_RELEASES.rows,
                .dataName = "cherry-doom",
                .slotPrefix = "chersav",
        },
        {
                .port = Port::dsda,
                .id = "dsda-doom",
                .name = "DSDA-Doom",
                .words = {"dsda"},
                .traits = &dialects::DSDA_TRAITS,
                .releases = dialects::DSDA_RELEASES.rows,
                .dataName = "dsda-doom",
                .slotPrefix = "dsda-doom-savegame",
        },
        {
                .port = Port::nyan,
                .id = "nyan-doom",
                .name = "Nyan Doom",
                .words = {"nyan"},
                .traits = &dialects::NYAN_TRAITS,
                .dataName = "nyan-doom",
                .slotPrefix = "nyan-doom-savegame",
        },
        {
                .port = Port::prboomPlus,
                .id = "prboom-plus",
                .name = "PrBoom+",
                .words = {"prboom-plus"},
                .traits = &dialects::PRBOOM_TRAITS,
                .dataName = "prboom-plus",
                .slotPrefix = "prboom-plus-savegame",
        },
        {
                .port = Port::glboomPlus,
                .name = "PrBoom+ (OpenGL)",
                .words = {"glboom-plus"},
                .traits = &dialects::GLBOOM_TRAITS,
                .dataName = "prboom-plus",
                .slotPrefix = "prboom-plus-savegame",
        },
        // The original PrBoom, which saves elsewhere.
        {
                .port = Port::prboom,
                .name = "PrBoom",
                .words = {"prboom"},
                .traits = &dialects::PRBOOM_TRAITS,
        },
        {
                .port = Port::glboom,
                .name = "PrBoom (OpenGL)",
                .words = {"glboom"},
                .traits = &dialects::GLBOOM_TRAITS,
        },
        {
                .port = Port::rude,
                .name = "Rude",
                .words = {"rude"},
                .traits = &dialects::RUDE_TRAITS,
        },
});

static_assert(KNOWN.size() == static_cast<size_t>(Port::rude) + 1, "a row for every port");
static_assert(std::ranges::all_of(KNOWN,
                                  [](const Known &each) { return &KNOWN[static_cast<size_t>(each.port)] == &each; }),
              "KNOWN in port order");

static_assert(std::ranges::all_of(KNOWN,
                                  [](const Known &each) {
                                      const bool wordless = std::ranges::all_of(
                                              each.words, [](const std::string_view word) { return word.empty(); });

                                      return wordless == (each.port == Port::unknown);
                                  }),
              "every port but unknown has a word");

constexpr bool clash(const Known &one, const Known &other) {
    const bool sameWord = one.match == other.match && std::ranges::any_of(one.words, [&other](const auto word) {
                              return !word.empty() && std::ranges::contains(other.words, word);
                          });

    return sameWord || (!one.id.empty() && one.id == other.id);
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

static_assert(distinct(), "no two ports share an id, or a word under the same rule");

bool matches(const Known &known, const std::string_view word, const std::string_view name, const bool exported) {
    switch (known.match) {
        case Match::contains:
            return name.contains(word);
        case Match::whole:
            return name == word;
        case Match::wholeOutsideFlatpak:
            break;
    }

    return !exported && name == word;
}

// The first port with a word in the name, or unknown.
const Known &named(const std::string_view name, const bool exported) {
    for (const Known &each : KNOWN) {
        if (std::ranges::any_of(each.words, [&](const std::string_view word) {
                return !word.empty() && matches(each, word, name, exported);
            })) {
            return each;
        }
    }

    return KNOWN.front();
}

}

std::span<const Known> known() {
    return KNOWN;
}

const Known &known(const Port port) {
    return KNOWN[static_cast<size_t>(port)];
}

const Known *withId(const std::string_view id) {
    if (id.empty()) {
        return nullptr;
    }

    // NOLINTNEXTLINE(readability-qualified-auto): the MSVC array iterator is a class, not a pointer.
    const auto found = std::ranges::find(KNOWN, id, &Known::id);

    return found == KNOWN.end() ? nullptr : &*found;
}

Port fromId(const std::string_view id) {
    const Known *found = withId(id);

    return found == nullptr ? Port::unknown : found->port;
}

const Known &of(const std::filesystem::path &file) {
    const bool exported = Detect::flatpakExport(file);
    const std::string_view runs = exported ? Detect::flatpakExecutable(file.filename().string()) : std::string_view();

    // Any other export is named by the last part of its app ID, such as io.github.fabiangreffrath.woof.
    const std::string name =
            !runs.empty() ? std::string(runs)
                          : Text::lower(exported && file.has_extension() ? file.extension().string().substr(1)
                                                                         : file.stem().string());

    return named(name, exported);
}

}
