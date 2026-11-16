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

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "ttk/system/Text.h"

#include "core/config/Options.h"
#include "core/launch/Dialect.h"
#include "core/launch/Flags.h"
#include "core/launch/Program.h"
#include "core/launch/dialects/Baseline.h"
#include "core/launch/dialects/Dialects.h"

using namespace ttk;

namespace Dialect {

namespace {

using dialects::Numbering;

// clang-format off
constexpr std::array CHOICES = std::to_array<Choice>({
        {Dialect::gzdoom,    "gzdoom",    "GZDoom",                    false, Program::Program::gzdoom},
        {Dialect::uzdoom,    "uzdoom",    "UZDoom",                    false, Program::Program::uzdoom},
        {Dialect::zdoom,     "zdoom",     "ZDoom 2.8",                 false, Program::Program::zdoom},
        {Dialect::zandronum, "zandronum", "Zandronum and Skulltag",    false, Program::Program::zandronum},
        {Dialect::dsda,      "dsda",      "DSDA-Doom and Nyan Doom",   false, Program::Program::dsda},
        {Dialect::prboom,    "prboom",    "PrBoom+",                   false, Program::Program::prboomPlus},
        {Dialect::woof,      "woof",      "Woof! and Nugget Doom",     false, Program::Program::woof},
        {Dialect::chocolate, "chocolate", "Chocolate and Crispy Doom", false, Program::Program::chocolate},
        {Dialect::eternity,  "eternity",  "Eternity Engine",           false, Program::Program::eternity},
        {Dialect::helion,    "helion",    "Helion",                    false, Program::Program::helion},
        {Dialect::boom,      "boom",      "Boom and MBF",              true,  Program::Program::boom},
        {Dialect::legacy,    "legacy",    "Doom Legacy",               true,  Program::Program::legacy},
        {Dialect::doom,      "doom",      "Vanilla Doom",              true,  Program::Program::doom},
});
// clang-format on

static_assert(CHOICES.size() == static_cast<size_t>(Dialect::doom) + 1, "a choice for every dialect");
static_assert(std::ranges::all_of(CHOICES,
                                  [](const Choice &each) {
                                      return &CHOICES[static_cast<size_t>(each.dialect)] == &each;
                                  }),
              "CHOICES in dialect order");

// What a program speaks inside DOSBox has none of these.
constexpr Features DOS_LACKS = Options::RULES | Feature::saveFolder | Feature::extraConfig | Feature::exec
                               | Feature::map | Feature::longtics | Feature::soloNet | Feature::levelstat
                               | Feature::shotdir | Feature::continues | Feature::recordsWithComplevel
                               | Feature::recordsWithSkill | Feature::continuesWithSkill;

const Numbering *takingLevels(const Complevels which) {
    switch (which) {
        case Complevels::prboom:
            return &dialects::PRBOOM_NUMBERING;
        case Complevels::dsda:
            return &dialects::DSDA_NUMBERING;
        case Complevels::woof:
            return &dialects::WOOF_NUMBERING;
        case Complevels::helion:
            return &dialects::HELION_NUMBERING;
        case Complevels::gameVersion:
            return &dialects::GAME_VERSION_NUMBERING;
        case Complevels::none:
            break;
    }

    return nullptr;
}

Port spokenBy(const Program::Known &known) {
    Port port = *known.port;
    port.program = known.program;

    return port;
}

Port bare(const Dialect dialect) {
    return spokenBy(Program::known(CHOICES[static_cast<size_t>(dialect)].program));
}

std::span<const VersionRange> releasesOf(const Port &port) {
    return Program::known(port.program).releases;
}

// A version no range names stays the newest release.
Port atVersion(const Port &port, const std::string_view version) {
    for (const VersionRange &each : releasesOf(port)) {
        if (each.first == version) {
            return atRelease(port, each);
        }
    }

    return port;
}

}

std::optional<bool> dosFromName(const std::filesystem::path &program) {
#ifdef _WIN32
    (void)program;

    return std::nullopt;
#else
    const std::string extension = Text::lower(program.extension().string());

    return extension == ".exe" || extension == ".com" || extension == ".bat" || extension == ".cmd";
#endif
}

std::span<const Choice> choices() {
    return CHOICES;
}

const Choice &choice(const Dialect dialect) {
    return CHOICES[static_cast<size_t>(dialect)];
}

const Choice *choice(const std::string_view word) {
    for (const Choice &each : CHOICES) {
        if (each.word == word) {
            return &each;
        }
    }

    return nullptr;
}

namespace {

// A guess for an unknown name says no dialect.
bool says(const Port &named, const Dialect dialect) {
    return named.has(Feature::recognised) && named.dialect == dialect;
}

}

Port of(const std::filesystem::path &program, const std::string_view picked) {
    const Port guessed = of(program);
    const Choice *wanted = choice(picked);

    if (wanted == nullptr || says(guessed, wanted->dialect)) {
        return guessed;
    }

    return bare(wanted->dialect);
}

std::string_view pickWord(const PortEntry &entry, const Dialect picked) {
    return says(of(std::filesystem::path(entry.file)), picked) ? std::string_view() : choice(picked).word;
}

Port of(const std::filesystem::path &program) {
    return spokenBy(Program::of(program));
}

std::span<const int> complevels(const Complevels which) {
    const Numbering *taking = takingLevels(which);

    return taking == nullptr ? std::span<const int>() : taking->levels;
}

std::string complevelWord(const Complevels which, const int level) {
    if (level < 0) {
        return {};
    }

    const Numbering *taking = takingLevels(which);

    if (taking != nullptr && taking->levelWord != nullptr) {
        return std::string(taking->levelWord(level));
    }

    return std::to_string(level);
}

namespace {

constexpr std::array LEVEL_NAMES = std::to_array<std::pair<int, std::string_view>>({
        {0, "Doom v1.2"},
        {1, "Doom v1.666"},
        {2, "Doom v1.9"},
        {3, "Ultimate Doom & Doom95"},
        {4, "Final Doom"},
        {5, "DOSDoom"},
        {6, "TASDoom"},
        {7, "Boom's inaccurate vanilla"},
        {8, "Boom v2.01"},
        {9, "Boom v2.02"},
        {10, "LxDoom"},
        {MBF, "MBF"},
        {12, "PrBoom v2.03 beta"},
        {13, "PrBoom v2.1.0-v2.1.1"},
        {14, "PrBoom v2.2.x"},
        {15, "PrBoom v2.3.x"},
        {16, "PrBoom v2.4.0"},
        {17, "PrBoom, current"},
        {MBF21, "MBF21"},
        {ID24, "id24"},
});

}

std::string_view levelName(const int level) {
    if (level < 0) {
        return PORTS_OWN;
    }

    if (level >= GAME_VERSIONS) {
        return dialects::gameVersionName(level);
    }

    for (const auto &[each, said] : LEVEL_NAMES) {
        if (each == level) {
            return said;
        }
    }

    return {};
}

int pickedLevel(const Complevels which, const int level) {
    const std::span<const int> offered = complevels(which);

    return level >= 0 && std::ranges::find(offered, level) != offered.end() ? level : -1;
}

std::string levelLabel(const Complevels which, const int level) {
    const int picked = pickedLevel(which, level);

    return picked < 0 ? std::string() : std::string(levelName(picked)) + " · " + complevelWord(which, picked);
}

Compatibility compatibility(const Port &port) {
    if (!Flags::modes(port.line).empty()) {
        return Compatibility::modes;
    }

    if (complevels(port.complevel).empty()) {
        return Compatibility::none;
    }

    return port.complevel == Complevels::gameVersion ? Compatibility::versions : Compatibility::complevels;
}

std::string_view levelTitle(const Compatibility kind) {
    return kind == Compatibility::versions ? "Game version" : "Complevel";
}

std::string_view levelHint(const Compatibility kind) {
    switch (kind) {
        case Compatibility::modes:
            return "The compatibility mode of the port. A mode fills compatflags and compatflags2, and Custom means "
                   "they match no mode";
        case Compatibility::versions:
            return "The Doom release the port plays as, passed on every launch, demos played back included";
        case Compatibility::complevels:
        case Compatibility::none:
            break;
    }

    return "The complevel, passed on every launch. A demo played back keeps its own";
}

bool isAutosave(const Port &port, const std::string_view file) {
    return !port.autosave.empty() && Text::iequals(file, port.autosave);
}

const Arg &arg(const Port &port, const Feature feature) {
    return dialects::resolve(port, feature);
}

std::string_view describe(const Port &port, const Feature feature) {
    const std::string_view own = arg(port, feature).description;

    return own.empty() ? dialects::baseline(feature).description : own;
}

bool add(std::vector<std::string> &args, const Port &port, const Feature feature) {
    const Arg &spelled = port.has(feature) ? arg(port, feature) : dialects::NO_ARG;

    if (spelled.name.empty()) {
        return false;
    }

    args.emplace_back(spelled.name);

    if (!spelled.word.empty()) {
        args.emplace_back(spelled.word);
    }

    return true;
}

bool add(std::vector<std::string> &args, const Port &port, const Feature feature, std::string value) {
    const bool added = add(args, port, feature);

    if (added) {
        args.push_back(std::move(value));
    }

    return added;
}

std::optional<int> parse(const Arg &arg, const std::string_view typed) {
    const std::optional<std::uint32_t> parsed = Flags::parse(typed);

    if (!parsed || std::cmp_less(*parsed, arg.range.least) || std::cmp_greater(*parsed, arg.range.most)) {
        return std::nullopt;
    }

    return static_cast<int>(*parsed);
}

bool worksAt(const Arg &arg, const int level) {
    const Levels &needs = arg.levels;

    if (needs.from < 0) {
        return true;
    }

    return level < 0 ? !needs.onLine : level >= needs.from && level <= needs.to;
}

Port of(const Config &config) {
    return of(config, config.activeProfile());
}

Port of(const Config &config, const Profile &profile) {
    const PortEntry *entry = config.findPort(profile.port);

    return entry == nullptr ? bare(Dialect::gzdoom) : of(*entry);
}

namespace {

Port spoken(const PortEntry &entry, const std::string_view picked) {
    Port port = of(std::filesystem::path(entry.file), picked);

    if (!entry.dosbox) {
        return entry.version.empty() ? port : atVersion(port, entry.version);
    }

    // Only the DOS dialects ever ran there, so any other name is answered for as vanilla.
    if (!port.has(Feature::recognised) || !choice(port.dialect).dos) {
        port = bare(Dialect::doom);
        port.features &= ~Features(Feature::recognised);
    }

    port.features |= Feature::dos;

    // Nothing after the last DOS release applies inside DOSBox.
    port.netgame = Netgames::none;
    port.complevel = Complevels::none;
    port.line = PortLine::none;
    port.features &= ~DOS_LACKS;

    return defined(port);
}

}

Port of(const PortEntry &entry) {
    // A downloaded port speaks what its program is named for.
    const std::string_view picked = entry.portId.empty() ? std::string_view(entry.dialect) : std::string_view();

    struct Asked {
        bool known{false};
        std::string file;
        std::string picked;
        bool dosbox{false};
        std::string version;
        Port port;
    };

    // One push asks for the same port many times over. Every view in a Port names a literal.
    // Assigned in place, so a miss reuses the room the strings already have.
    thread_local Asked last;

    if (!last.known || last.dosbox != entry.dosbox || last.file != entry.file || last.picked != picked
        || last.version != entry.version) {
        last.known = true;
        last.file = entry.file;
        last.picked = picked;
        last.dosbox = entry.dosbox;
        last.version = entry.version;
        last.port = spoken(entry, picked);
    }

    return last.port;
}

std::span<const VersionRange> versions(const PortEntry &entry) {
    return releasesOf(of(entry));
}

bool needsVersion(const PortEntry &entry) {
    if (entry.dosbox || !entry.portId.empty()) {
        return false;
    }

    const std::span<const VersionRange> ranges = versions(entry);

    return !ranges.empty()
           && std::ranges::none_of(ranges, [&entry](const VersionRange &each) { return each.first == entry.version; });
}

}
