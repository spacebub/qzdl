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

#include "core/launch/Program.h"
#include "core/launch/dialects/Dialects.h"
#include "core/ports/Detect.h"

using namespace ttk;

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
                .portId = "doomlegacy",
                .name = "Doom Legacy",
                .words = {"doom3", "legacy", "doomlegacy"},
                .match = Match::wholeOutsideFlatpak,
                .port = &dialects::LEGACY_PORT,
        },
        {
                .program = Program::helion,
                .portId = "helion",
                .name = "Helion",
                .words = {"helion"},
                .port = &dialects::HELION_PORT,
                .dataName = "Helion",
        },
        // Whole name: gzdoom contains zdoom.
        {
                .program = Program::zdoom,
                .portId = "zdoom",
                .name = "ZDoom",
                .words = {"zdoom"},
                .match = Match::whole,
                .port = &dialects::ZDOOM_PORT,
                .dataName = "ZDoom",
        },
        {
                .program = Program::zandronum,
                .portId = "zandronum",
                .name = "Zandronum",
                .words = {"zandronum"},
                .port = &dialects::ZANDRONUM_PORT,
                .dataName = "Zandronum",
        },
        {
                .program = Program::skulltag,
                .name = "Skulltag",
                .words = {"skulltag"},
                .port = &dialects::ZANDRONUM_PORT,
        },
        {
                .program = Program::uzdoom,
                .portId = "uzdoom",
                .name = "UZDoom",
                .words = {"uzdoom"},
                .port = &dialects::UZDOOM_PORT,
                .releases = dialects::UZDOOM_RELEASES,
                .dataName = "UZDoom",
        },
        {
                .program = Program::gzdoom,
                .portId = "gzdoom",
                .name = "GZDoom",
                .words = {"gzdoom"},
                .port = &dialects::GZDOOM_PORT,
                .releases = dialects::GZDOOM_RELEASES,
                .dataName = "GZDoom",
        },
        {
                .program = Program::vkdoom,
                .name = "VKDoom",
                .words = {"vkdoom"},
                .port = &dialects::GZDOOM_PORT,
                .dataName = "VKDoom",
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
                .portId = "crispy-doom",
                .name = "Crispy Doom",
                .words = {"crispy"},
                .port = &dialects::CRISPY_PORT,
                .dataName = "crispy-doom",
                .slotPrefix = "doomsav",
        },
        {
                .program = Program::chocolateOther,
                .words = {"chocolate-heretic", "chocolate-hexen", "chocolate-strife"},
                .port = &dialects::CHOCOLATE_OTHER,
        },
        {
                .program = Program::chocolate,
                .portId = "chocolate-doom",
                .name = "Chocolate Doom",
                .words = {"chocolate"},
                .port = &dialects::CHOCOLATE_PORT,
                .dataName = "chocolate-doom",
                .slotPrefix = "doomsav",
        },
        // Whole name: prboom contains boom.
        {
                .program = Program::boom,
                .portId = "boom",
                .name = "Boom",
                .words = {"boom"},
                .match = Match::whole,
                .port = &dialects::BOOM_PORT,
        },
        // MBF and WinMBF are the same line, predating -complevel and everything after it.
        {
                .program = Program::mbf,
                .portId = "mbf",
                .name = "MBF",
                .words = {"mbf"},
                .port = &dialects::MBF_PORT,
        },
        {
                .program = Program::eternity,
                .name = "Eternity Engine",
                .words = {"eternity"},
                .port = &dialects::ETERNITY_PORT,
        },
        {
                .program = Program::woof,
                .portId = "woof",
                .name = "Woof!",
                .words = {"woof"},
                .port = &dialects::WOOF_PORT,
                .dataName = "woof",
                .slotPrefix = "woofsav",
        },
        {
                .program = Program::nugget,
                .portId = "nugget-doom",
                .name = "Nugget Doom",
                .words = {"nugget"},
                .port = &dialects::NUGGET_PORT,
                .dataName = "nugget-doom",
                .slotPrefix = "nuggsav",
        },
        {
                .program = Program::cherry,
                .name = "Cherry Doom",
                .words = {"cherry"},
                .port = &dialects::CHERRY_PORT,
        },
        {
                .program = Program::dsda,
                .portId = "dsda-doom",
                .name = "DSDA-Doom",
                .words = {"dsda"},
                .port = &dialects::DSDA_PORT,
                .releases = dialects::DSDA_RELEASES,
                .dataName = "dsda-doom",
                .slotPrefix = "dsda-doom-savegame",
        },
        {
                .program = Program::nyan,
                .name = "Nyan Doom",
                .words = {"nyan"},
                .port = &dialects::NYAN_PORT,
                .dataName = "nyan-doom",
                .slotPrefix = "nyan-doom-savegame",
        },
        {
                .program = Program::prboomPlus,
                .portId = "prboom-plus",
                .name = "PrBoom+",
                .words = {"prboom-plus"},
                .port = &dialects::PRBOOM_PORT,
                .dataName = "prboom-plus",
                .slotPrefix = "prboom-plus-savegame",
        },
        {
                .program = Program::glboomPlus,
                .name = "PrBoom+ (OpenGL)",
                .words = {"glboom-plus"},
                .port = &dialects::GLBOOM_PORT,
                .dataName = "prboom-plus",
                .slotPrefix = "prboom-plus-savegame",
        },
        // The original PrBoom, which saves elsewhere.
        {
                .program = Program::prboom,
                .name = "PrBoom",
                .words = {"prboom"},
                .port = &dialects::PRBOOM_PORT,
        },
        {
                .program = Program::glboom,
                .name = "PrBoom (OpenGL)",
                .words = {"glboom"},
                .port = &dialects::GLBOOM_PORT,
        },
        {
                .program = Program::rude,
                .name = "Rude",
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
                                      const bool wordless = std::ranges::all_of(
                                              each.words, [](const std::string_view word) { return word.empty(); });

                                      return wordless == (each.program == Program::unknown);
                                  }),
              "every program but unknown has a word");

constexpr bool clash(const Known &one, const Known &other) {
    const bool sameWord = one.match == other.match && std::ranges::any_of(one.words, [&other](const auto word) {
                              return !word.empty() && std::ranges::contains(other.words, word);
                          });

    return sameWord || (!one.portId.empty() && one.portId == other.portId);
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

// The first program with a word in the name, or unknown.
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

const Known &known(const Program program) {
    return KNOWN[static_cast<size_t>(program)];
}

const Known &of(const std::filesystem::path &file) {
    const bool exported = Detect::flatpakExport(file);
    const std::string_view runs = exported ? Detect::flatpakProgram(file.filename().string()) : std::string_view();

    // Any other export is named by the last part of its app ID, such as io.github.fabiangreffrath.woof.
    const std::string name =
            !runs.empty() ? std::string(runs)
                          : Text::lower(exported && file.has_extension() ? file.extension().string().substr(1)
                                                                         : file.stem().string());

    return named(name, exported);
}

}
