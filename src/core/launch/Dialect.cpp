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
#include "core/launch/KnownPorts.h"
#include "core/launch/dialects/Baseline.h"
#include "core/launch/dialects/Dialects.h"

using namespace ttk;

namespace Dialect {

namespace {

using dialects::Numbering;

struct Lead {
    Dialect dialect;

    // Spoken by ports run inside DOSBox.
    bool dos;

    // The port the dialect is named for.
    Port::Port port;
};

// In the order a pick list shows the dialects.
constexpr std::array LEADS = std::to_array<Lead>({
        {.dialect = Dialect::gzdoom, .dos = false, .port = Port::Port::gzdoom},
        {.dialect = Dialect::uzdoom, .dos = false, .port = Port::Port::uzdoom},
        {.dialect = Dialect::zdoom, .dos = false, .port = Port::Port::zdoom},
        {.dialect = Dialect::zandronum, .dos = false, .port = Port::Port::zandronum},
        {.dialect = Dialect::dsda, .dos = false, .port = Port::Port::dsda},
        {.dialect = Dialect::prboom, .dos = false, .port = Port::Port::prboomPlus},
        {.dialect = Dialect::woof, .dos = false, .port = Port::Port::woof},
        {.dialect = Dialect::chocolate, .dos = false, .port = Port::Port::chocolate},
        {.dialect = Dialect::eternity, .dos = false, .port = Port::Port::eternity},
        {.dialect = Dialect::helion, .dos = false, .port = Port::Port::helion},
        {.dialect = Dialect::boom, .dos = true, .port = Port::Port::boom},
        {.dialect = Dialect::legacy, .dos = true, .port = Port::Port::legacy},
        {.dialect = Dialect::doom, .dos = true, .port = Port::Port::doom},
});

static_assert(LEADS.size() == static_cast<size_t>(Dialect::doom) + 1, "a lead for every dialect");
static_assert(std::ranges::all_of(LEADS,
                                  [](const Lead &each) { return &LEADS[static_cast<size_t>(each.dialect)] == &each; }),
              "LEADS in dialect order");

const Lead &leadOf(const Dialect dialect) {
    return LEADS[static_cast<size_t>(dialect)];
}

// What a port speaks inside DOSBox has none of these.
constexpr Features DOS_LACKS = Options::RULES | Feature::saveFolder | Feature::extraConfig | Feature::exec
                               | Feature::map | Feature::longtics | Feature::soloNet | Feature::levelstat
                               | Feature::shotdir | Feature::continues | Feature::recordsWithComplevel
                               | Feature::recordsWithSkill | Feature::continuesWithSkill;

const Numbering *takingLevels(const Complevels which) {
    switch (which) {
        case Complevels::gameVersion19:
        case Complevels::gameVersion1666:
        case Complevels::gameVersion12:
        case Complevels::gameVersion:
            return &dialects::GAME_VERSION_NUMBERINGS.at(dialects::listPlace(which));
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
        case Complevels::none:
            break;
    }

    return nullptr;
}

Traits spokenBy(const Port::Known &known) {
    Traits traits = *known.traits;
    traits.port = known.port;

    return traits;
}

Traits bare(const Dialect dialect) {
    return spokenBy(Port::known(leadOf(dialect).port));
}

// Each lead, then the other ports that speak its dialect.
std::vector<Port::Port> picksOf(const bool dos) {
    std::vector<Port::Port> out;

    for (const Lead &each : LEADS) {
        if (each.dos != dos) {
            continue;
        }

        out.push_back(each.port);

        for (const Port::Known &known : Port::known()) {
            if (!known.id.empty() && known.port != each.port && known.traits->dialect == each.dialect) {
                out.push_back(known.port);
            }
        }
    }

    return out;
}

std::span<const VersionRange> releasesOf(const Traits &traits) {
    return Port::known(traits.port).releases;
}

// A version without a release stays the newest release.
Traits atVersion(const Traits &traits, const std::string_view version) {
    const VersionRange *range = rangeOf(releasesOf(traits), version);

    return range == nullptr ? traits : atRelease(traits, *range);
}

}

std::optional<bool> dosFromName(const std::filesystem::path &file) {
#ifdef _WIN32
    (void)file;

    return std::nullopt;
#else
    const std::string extension = Text::lower(file.extension().string());

    return extension == ".exe" || extension == ".com" || extension == ".bat" || extension == ".cmd";
#endif
}

std::span<const Port::Port> picks(const bool dos) {
    static const std::vector<Port::Port> native = picksOf(false);
    static const std::vector<Port::Port> underDosbox = picksOf(true);

    return dos ? underDosbox : native;
}

Traits of(const std::filesystem::path &file, const Port::Port picked) {
    return picked == Port::Port::unknown ? of(file) : spokenBy(Port::known(picked));
}

Port::Port lead(const Dialect dialect) {
    return leadOf(dialect).port;
}

Port::Port fromName(const Engine &engine) {
    Engine unpicked = engine;
    unpicked.port = Port::Port::unknown;

    const Traits traits = of(unpicked);
    const Port::Known &known = Port::known(traits.port);

    if (!known.name.empty()) {
        return traits.port;
    }

    // A fork row speaks the traits of a named port, as glboomx speaks GLBoom+.
    for (const Port::Known &each : Port::known()) {
        if (each.traits == known.traits && !each.name.empty()) {
            return each.port;
        }
    }

    return lead(traits.dialect);
}

Traits of(const std::filesystem::path &file) {
    return spokenBy(Port::of(file));
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

Compatibility compatibility(const Traits &traits) {
    if (!Flags::modes(traits.line).empty()) {
        return Compatibility::modes;
    }

    if (complevels(traits.complevel).empty()) {
        return Compatibility::none;
    }

    return dialects::listsGameVersions(traits.complevel) ? Compatibility::versions : Compatibility::complevels;
}

std::string_view levelTitle(const Compatibility kind) {
    return kind == Compatibility::versions ? "Game version" : "Complevel";
}

std::string_view levelHint(const Traits &traits) {
    switch (compatibility(traits)) {
        case Compatibility::modes:
            return Flags::modeHint(traits.line);
        case Compatibility::versions:
            return "The Doom release the port plays as, passed on every launch, demos played back included";
        case Compatibility::complevels:
        case Compatibility::none:
            break;
    }

    return "The complevel, passed on every launch. A demo played back keeps its own";
}

bool isAutosave(const Traits &traits, const std::string_view file) {
    return !traits.autosave.empty() && Text::iequals(file, traits.autosave);
}

const Arg &arg(const Traits &traits, const Feature feature) {
    return dialects::resolve(traits, feature);
}

std::string_view describe(const Traits &traits, const Feature feature) {
    const std::string_view own = arg(traits, feature).description;

    return own.empty() ? dialects::baseline(feature).description : own;
}

bool add(std::vector<std::string> &args, const Traits &traits, const Feature feature) {
    const Arg &spelled = traits.has(feature) ? arg(traits, feature) : dialects::NO_ARG;

    if (spelled.name.empty()) {
        return false;
    }

    args.emplace_back(spelled.name);

    if (!spelled.word.empty()) {
        args.emplace_back(spelled.word);
    }

    return true;
}

bool add(std::vector<std::string> &args, const Traits &traits, const Feature feature, std::string value) {
    const bool added = add(args, traits, feature);

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

Traits of(const Config &config) {
    return of(config, config.activeProfile());
}

Traits of(const Config &config, const Profile &profile) {
    const Engine *engine = config.findEngine(profile.engine);

    return engine == nullptr ? bare(Dialect::gzdoom) : of(*engine);
}

namespace {

Traits spoken(const Engine &engine) {
    Traits traits = of(std::filesystem::path(engine.file), engine.port);

    // ZDL4 installs the latest release of a managed port, whatever the engine stores.
    if (!engine.dosbox) {
        return engine.managed ? traits : atVersion(traits, engine.version);
    }

    // Only the DOS dialects ever ran there, so any other name is answered for as vanilla.
    if (!traits.has(Feature::recognised) || !leadOf(traits.dialect).dos) {
        traits = bare(Dialect::doom);
        traits.features &= ~Features(Feature::recognised);
    }

    traits.features |= Feature::dos;

    // Nothing after the last DOS release applies inside DOSBox.
    traits.netgame = Netgames::none;
    traits.complevel = Complevels::none;
    traits.line = PortLine::none;
    traits.features &= ~DOS_LACKS;

    return defined(traits);
}

}

Traits of(const Engine &engine) {
    struct Asked {
        bool known{false};
        AnswerKey key;
        Traits traits;
    };

    // One push asks for the same port many times over. Every view in a Traits names a literal.
    // Assigned in place, so a miss reuses the room the strings already have.
    thread_local Asked last;

    if (!last.known || last.key != answerKey(engine)) {
        last.known = true;
        last.key = answerKey(engine);
        last.traits = spoken(engine);
    }

    return last.traits;
}

std::span<const VersionRange> versions(const Engine &engine) {
    return releasesOf(of(engine));
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

bool needsVersion(const Engine &engine) {
    if (engine.dosbox || engine.managed) {
        return false;
    }

    return !versions(engine).empty() && leadingRelease(engine.version).empty();
}

std::string suggestedVersion(const Engine &engine, const VersionReader read) {
    if (!needsVersion(engine)) {
        return {};
    }

    const std::string text = read(std::filesystem::path(engine.file));
    const std::string_view release = leadingRelease(text);

    // A lone number such as the 4 of "4, 14, 2, 0" is no release.
    return release.contains('.') ? std::string(release) : std::string();
}

}
