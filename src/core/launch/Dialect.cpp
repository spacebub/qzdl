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
#include <span>
#include <string>
#include <utility>
#include <vector>

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

struct Lead {
    Dialect dialect;

    // Spoken by programs run inside DOSBox.
    bool dos;

    // The program the dialect is named for.
    Program::Program program;
};

// In the order a pick list shows the dialects.
constexpr std::array LEADS = std::to_array<Lead>({
        {.dialect = Dialect::gzdoom, .dos = false, .program = Program::Program::gzdoom},
        {.dialect = Dialect::uzdoom, .dos = false, .program = Program::Program::uzdoom},
        {.dialect = Dialect::zdoom, .dos = false, .program = Program::Program::zdoom},
        {.dialect = Dialect::zandronum, .dos = false, .program = Program::Program::zandronum},
        {.dialect = Dialect::dsda, .dos = false, .program = Program::Program::dsda},
        {.dialect = Dialect::prboom, .dos = false, .program = Program::Program::prboomPlus},
        {.dialect = Dialect::woof, .dos = false, .program = Program::Program::woof},
        {.dialect = Dialect::chocolate, .dos = false, .program = Program::Program::chocolate},
        {.dialect = Dialect::eternity, .dos = false, .program = Program::Program::eternity},
        {.dialect = Dialect::helion, .dos = false, .program = Program::Program::helion},
        {.dialect = Dialect::boom, .dos = true, .program = Program::Program::boom},
        {.dialect = Dialect::legacy, .dos = true, .program = Program::Program::legacy},
        {.dialect = Dialect::doom, .dos = true, .program = Program::Program::doom},
});

static_assert(LEADS.size() == static_cast<size_t>(Dialect::doom) + 1, "a lead for every dialect");
static_assert(std::ranges::all_of(LEADS,
                                  [](const Lead &each) { return &LEADS[static_cast<size_t>(each.dialect)] == &each; }),
              "LEADS in dialect order");

const Lead &leadOf(const Dialect dialect) {
    return LEADS[static_cast<size_t>(dialect)];
}

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
        case Complevels::woofToMbf21:
            return &dialects::WOOF_TO_MBF21_NUMBERING;
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
    return spokenBy(Program::known(leadOf(dialect).program));
}

// Each lead, then the other programs that speak its dialect.
std::vector<Program::Program> picksOf(const bool dos) {
    std::vector<Program::Program> out;

    for (const Lead &each : LEADS) {
        if (each.dos != dos) {
            continue;
        }

        out.push_back(each.program);

        for (const Program::Known &known : Program::known()) {
            if (!known.id.empty() && known.program != each.program && known.port->dialect == each.dialect) {
                out.push_back(known.program);
            }
        }
    }

    return out;
}

std::span<const VersionRange> releasesOf(const Port &port) {
    return Program::known(port.program).releases;
}

// A version without a release stays the newest release.
Port atVersion(const Port &port, const std::string_view version) {
    const VersionRange *range = rangeOf(releasesOf(port), version);

    return range == nullptr ? port : atRelease(port, *range);
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

std::span<const Program::Program> picks(const bool dos) {
    static const std::vector<Program::Program> native = picksOf(false);
    static const std::vector<Program::Program> underDosbox = picksOf(true);

    return dos ? underDosbox : native;
}

Port of(const std::filesystem::path &program, const Program::Program picked) {
    return picked == Program::Program::unknown ? of(program) : spokenBy(Program::known(picked));
}

Program::Program lead(const Dialect dialect) {
    return leadOf(dialect).program;
}

Program::Program fromName(const PortEntry &entry) {
    PortEntry unpicked = entry;
    unpicked.program = Program::Program::unknown;

    const Port port = of(unpicked);

    return Program::known(port.program).name.empty() ? lead(port.dialect) : port.program;
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

std::string_view levelHint(const Port &port) {
    switch (compatibility(port)) {
        case Compatibility::modes:
            return Flags::modeHint(port.line);
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

Port spoken(const PortEntry &entry) {
    Port port = of(std::filesystem::path(entry.file), entry.program);

    // ZDL4 installs the latest release of a managed port, whatever the entry stores.
    if (!entry.dosbox) {
        return entry.managed ? port : atVersion(port, entry.version);
    }

    // Only the DOS dialects ever ran there, so any other name is answered for as vanilla.
    if (!port.has(Feature::recognised) || !leadOf(port.dialect).dos) {
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
    struct Asked {
        bool known{false};
        AnswerKey key;
        Port port;
    };

    // One push asks for the same port many times over. Every view in a Port names a literal.
    // Assigned in place, so a miss reuses the room the strings already have.
    thread_local Asked last;

    if (!last.known || last.key != answerKey(entry)) {
        last.known = true;
        last.key = answerKey(entry);
        last.port = spoken(entry);
    }

    return last.port;
}

std::span<const VersionRange> versions(const PortEntry &entry) {
    return releasesOf(of(entry));
}

std::string_view leadingRelease(const std::string_view text) {
    const size_t start = text.find_first_of("0123456789");

    if (start == std::string_view::npos) {
        return {};
    }

    const auto part = [](const char each) {
        return (each >= '0' && each <= '9') || each == '.' || (each >= 'a' && each <= 'z')
               || (each >= 'A' && each <= 'Z');
    };

    size_t end = start;

    while (end < text.size() && part(text[end])) {
        ++end;
    }

    std::string_view release = text.substr(start, end - start);

    while (release.ends_with('.')) {
        release.remove_suffix(1);
    }

    return release;
}

const VersionRange *rangeOf(const std::span<const VersionRange> ranges, const std::string_view version) {
    const std::string_view release = leadingRelease(version);

    if (release.empty() || ranges.empty()) {
        return nullptr;
    }

    const auto reached =
            std::ranges::find_if(ranges, [release](const VersionRange &each) { return !later(each.first, release); });

    return reached == ranges.end() ? &ranges.back() : &*reached;
}

bool needsVersion(const PortEntry &entry) {
    if (entry.dosbox || entry.managed) {
        return false;
    }

    return !versions(entry).empty() && leadingRelease(entry.version).empty();
}

std::string suggestedVersion(const PortEntry &entry, const VersionReader read) {
    if (!needsVersion(entry)) {
        return {};
    }

    const std::string text = read(std::filesystem::path(entry.file));
    const std::string_view release = leadingRelease(text);

    // A lone number such as the 4 of "4, 14, 2, 0" is no release.
    return release.contains('.') ? std::string(release) : std::string();
}

}
