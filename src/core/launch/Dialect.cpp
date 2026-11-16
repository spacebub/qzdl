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
#include "core/launch/dialects/Baseline.h"
#include "core/launch/dialects/Dialects.h"
#include "core/ports/Detect.h"

using namespace ttk;

namespace Dialect {

namespace {

using dialects::Definition;

constexpr std::array CHOICES = {
        Choice{.id = Id::gzdoom, .word = "gzdoom", .name = "GZDoom", .dos = false},
        Choice{.id = Id::uzdoom, .word = "uzdoom", .name = "UZDoom", .dos = false},
        Choice{.id = Id::zdoom, .word = "zdoom", .name = "ZDoom 2.8", .dos = false},
        Choice{.id = Id::zandronum, .word = "zandronum", .name = "Zandronum and Skulltag", .dos = false},
        Choice{.id = Id::dsda, .word = "dsda", .name = "DSDA-Doom and Nyan Doom", .dos = false},
        Choice{.id = Id::prboom, .word = "prboom", .name = "PrBoom+", .dos = false},
        Choice{.id = Id::woof, .word = "woof", .name = "Woof! and Nugget Doom", .dos = false},
        Choice{.id = Id::chocolate, .word = "chocolate", .name = "Chocolate and Crispy Doom", .dos = false},
        Choice{.id = Id::eternity, .word = "eternity", .name = "Eternity Engine", .dos = false},
        Choice{.id = Id::helion, .word = "helion", .name = "Helion", .dos = false},
        Choice{.id = Id::boom, .word = "boom", .name = "Boom and MBF", .dos = true},
        Choice{.id = Id::legacy, .word = "legacy", .name = "Doom Legacy", .dos = true},
        Choice{.id = Id::doom, .word = "doom", .name = "Vanilla Doom", .dos = true},
};

static_assert(CHOICES.size() == static_cast<size_t>(Id::doom) + 1, "a choice for every Id");
static_assert(std::ranges::all_of(CHOICES,
                                  [](const Choice &each) { return &CHOICES[static_cast<size_t>(each.id)] == &each; }),
              "CHOICES in Id order");

// The order a program name is tried against the dialects. The first to claim it wins.
constexpr std::array ORDER = {
        &dialects::DOOM,
        &dialects::LEGACY,
        &dialects::HELION,
        &dialects::ZDOOM,
        &dialects::ZANDRONUM,
        &dialects::UZDOOM,
        &dialects::GZDOOM,
        &dialects::CHOCOLATE,
        &dialects::BOOM,
        &dialects::ETERNITY,
        &dialects::WOOF,
        &dialects::DSDA,
        &dialects::PRBOOM,
};

static_assert(ORDER.size() == CHOICES.size(), "every dialect has a place in the order");

// What a program speaks inside DOSBox has none of these.
constexpr Features DOS_LACKS = Options::FEATURES | Feature::extraConfig | Feature::exec | Feature::map
                               | Feature::longtics | Feature::soloNet | Feature::levelstat | Feature::shotdir
                               | Feature::continues | Feature::recordsWithComplevel | Feature::recordsWithSkill
                               | Feature::continuesWithSkill;

const Definition &definitionOf(const Id id) {
    return **std::ranges::find(ORDER, id, [](const Definition *each) { return each->id; });
}

const Definition *takingLevels(const Complevels which) {
    if (which == Complevels::none) {
        return nullptr;
    }

    // NOLINTNEXTLINE(readability-qualified-auto): the MSVC array iterator is a class, not a pointer.
    const auto found = std::ranges::find(ORDER, which, [](const Definition *each) { return each->bare.complevel; });

    return found == ORDER.end() ? nullptr : *found;
}

std::span<const VersionRange> releasesOf(const Port &port) {
    const Definition &definition = definitionOf(port.id);

    return port.line == definition.bare.line ? definition.releases : std::span<const VersionRange>();
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

const Choice &choice(const Id id) {
    return CHOICES[static_cast<size_t>(id)];
}

const Choice *choice(const std::string_view word) {
    for (const Choice &each : CHOICES) {
        if (each.word == word) {
            return &each;
        }
    }

    return nullptr;
}

Port of(const std::filesystem::path &program, const std::string_view picked) {
    const Port guessed = of(program);
    const Choice *wanted = choice(picked);

    if (wanted == nullptr || (guessed.has(Feature::recognised) && guessed.id == wanted->id)) {
        return guessed;
    }

    return definitionOf(wanted->id).bare;
}

Port of(const std::filesystem::path &program) {
    const bool exported = Detect::flatpakExport(program);
    const std::string_view runs = exported ? Detect::flatpakProgram(program.filename().string()) : std::string_view();

    // Any other export is named by the last part of its app ID, such as io.github.fabiangreffrath.woof.
    // No DOS release comes as one.
    const std::string name =
            !runs.empty() ? std::string(runs)
                          : Text::lower(exported && program.has_extension() ? program.extension().string().substr(1)
                                                                            : program.stem().string());
    const dialects::Name asked{.text = name, .exported = exported};

    for (const Definition *each : ORDER) {
        if (std::optional<Port> found = each->named(asked)) {
            return *found;
        }
    }

    Port guessed = dialects::GZDOOM.bare;
    guessed.features &= ~Features(Feature::recognised);

    return guessed;
}

std::span<const int> complevels(const Complevels which) {
    const Definition *taking = takingLevels(which);

    return taking == nullptr ? std::span<const int>() : taking->levels;
}

std::string complevelWord(const Complevels which, const int level) {
    if (level < 0) {
        return {};
    }

    const Definition *taking = takingLevels(which);

    if (taking != nullptr && taking->levelWord != nullptr) {
        return std::string(taking->levelWord(level));
    }

    return std::to_string(level);
}

std::string_view complevelSwitch(const Complevels which) {
    const Definition *taking = takingLevels(which);

    return taking == nullptr ? std::string_view("-complevel") : taking->levelSwitch;
}

std::string_view versionName(const int level) {
    return dialects::gameVersionName(level);
}

bool isAutosave(const Port &port, const std::string_view file) {
    return !port.autosave.empty() && Text::iequals(file, port.autosave);
}

const Arg &arg(const Port &port, const Feature feature) {
    return dialects::spelling(port, feature);
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

    return entry == nullptr ? dialects::GZDOOM.bare : of(*entry);
}

namespace {

Port spoken(const PortEntry &entry, const std::string_view picked) {
    Port port = of(std::filesystem::path(entry.file), picked);

    if (!entry.dosbox) {
        return entry.version.empty() ? port : atVersion(port, entry.version);
    }

    // Only the DOS dialects ever ran there, so any other name is answered for as vanilla.
    if (!port.has(Feature::recognised) || !choice(port.id).dos) {
        port = dialects::DOOM.bare;
        port.features &= ~Features(Feature::recognised);
    }

    port.features |= Feature::dos;

    // Nothing after the last DOS release applies inside DOSBox.
    port.save = "";
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
