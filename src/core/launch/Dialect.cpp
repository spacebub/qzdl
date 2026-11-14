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
#include <optional>
#include <string>
#include <utility>

#include "ttk/system/Text.h"

#include "core/launch/Dialect.h"

using namespace ttk;

namespace Dialect {

namespace {

constexpr std::string_view DEATHMATCH3 = "-dm3";

constexpr Port ZDOOM{};

constexpr Port BOOM{
        .id = Id::prboom,
        .save = "-save",
        .loads = SaveNames::slot,
        .saveExt = ".dsg",
        .bex = "-deh",
        .exec = false,
        .map = false,
        .longWarp = true,
        .netgame = Netgames::prboom,
        .fastdemo = true,
        .complevel = Complevels::prboom,
        .longtics = true,
        .soloNet = true,
        .levelstat = true,
        .pistolStart = PistolStart::flag,
        .cheats = false,
        .noAutoload = "-noload",
        .display = Windowing::noWindow,
        .displayKept = false,
        .teamPick = false,
        .continues = true,
        .line = PortLine::prboom,
};

// -loadgame needs 0.30.0 or later. Releases from 0.25.0 on stop on a switch they do not know.
constexpr Port DSDA{
        .id = Id::dsda,
        .save = "-save",
        .loads = SaveNames::slot,
        .saveExt = ".dsg",
        .lastSlot = 118,
        .configName = "dsda",
        .bex = "-deh",
        .exec = false,
        .map = false,
        .longWarp = true,
        .netgame = Netgames::none,
        .fastdemo = true,
        .complevel = Complevels::dsda,
        .recordsWithComplevel = true,
        .longtics = true,
        .soloNet = true,
        .levelstat = true,
        .pistolStart = PistolStart::flag,
        .cheats = false,
        .display = Windowing::switches,
        .displayKept = false,
        .teamPick = false,
        .continues = true,
        .continueComplevel = true,
        .line = PortLine::dsda,
};

constexpr Port WOOF{
        .id = Id::woof,
        .save = "-save",
        .loads = SaveNames::slot,
        .saveExt = ".dsg",
        .bex = "-deh",
        .exec = false,
        .map = false,
        .longWarp = true,
        .netgame = Netgames::chocolate,
        .deathmatch3 = DEATHMATCH3,
        .fastdemo = true,
        .complevel = Complevels::woof,
        .recordsWithComplevel = true,
        .unrecordable = ID24,
        .longtics = true,
        .soloNet = true,
        .levelstat = true,
        .pistolStart = PistolStart::flag,
        .cheats = false,
        .display = Windowing::switches,
        .teamPick = false,
        .continues = true,
        .continueComplevel = true,
        .line = PortLine::woof,
};

constexpr Port ETERNITY{
        .id = Id::eternity,
        .save = "-save",
        .loads = SaveNames::slot,
        .saveExt = ".dsg",
        .configName = "eternity",
        .exec = false,
        .map = false,
        .longWarp = true,
        .netgame = Netgames::none,
        .fastdemo = true,
        .soloNet = true,
        .pistolStart = PistolStart::none,
        .cheats = false,
        .noAutoload = "-noload",
        // Only -geom, which needs a size as well.
        .display = Windowing::none,
        .shotdir = false,
        .teamPick = false,
        .line = PortLine::eternity,
};

constexpr Port BOOM202{
        .id = Id::boom,
        .save = "-save",
        .loads = SaveNames::slot,
        .saveExt = ".dsg",
        .configFile = false,
        .configName = "mbf",
        .bex = "-deh",
        .exec = false,
        .map = false,
        .netgame = Netgames::none,
        .fastdemo = true,
        .pistolStart = PistolStart::none,
        .cheats = false,
        // MBF skips its preloaded files. Boom has none, and passes over a switch it does not know.
        .noAutoload = "-noload",
        .display = Windowing::none,
        .shotdir = false,
        .teamPick = false,
        .line = PortLine::none,
};

constexpr Port VANILLA{
        .id = Id::chocolate,
        .loads = SaveNames::slot,
        .saveExt = ".dsg",
        .extraConfig = true,
        .configName = "chocolate",
        .pwads = "-merge",
        .bex = "-deh",
        .exec = false,
        .map = false,
        .netgame = Netgames::chocolate,
        .complevel = Complevels::gameVersion,
        .longtics = true,
        .soloNet = true,
        .pistolStart = PistolStart::none,
        .cheats = false,
        .display = Windowing::switches,
        .shotdir = false,
        .teamPick = false,
        .line = PortLine::chocolate,
};

constexpr Port HELION{
        .id = Id::helion,
        .loads = SaveNames::name,
        .saveExt = ".hsg",
        .configName = "helion",
        .bex = "-deh",
        .exec = false,
        .longWarp = true,
        .respawn = false,
        .netgame = Netgames::none,
        .timedemo = false,
        .complevel = Complevels::helion,
        .soloNet = true,
        .recordAddsLmp = false,
        .levelstat = true,
        .pistolStart = PistolStart::flag,
        .cheats = false,
        .noAutoload = "",
        .nosfx = "",
        .display = Windowing::none,
        .shotdir = false,
        .teamPick = false,
        .line = PortLine::helion,
};

// Takes -config but writes to it without ever reading it back, so it keeps
// $DOOMWADDIR/config.cfg instead.
constexpr Port LEGACY{
        .id = Id::legacy,
        .iwad = false,
        .save = "",
        .loads = SaveNames::slot,
        .saveExt = ".dsg",
        .configFile = false,
        .deh = "-dehacked",
        .bex = "-dehacked",
        .exec = false,
        .map = false,
        .netgame = Netgames::none,
        .pistolStart = PistolStart::none,
        .cheats = false,
        .noAutoload = "",
        // Its -nosound leaves the music on.
        .nosfx = "-nosound",
        .display = Windowing::none,
        .shotdir = false,
        .teamPick = false,
        .line = PortLine::none,
};

constexpr Port DOOM{
        .id = Id::doom,
        .iwad = false,
        .save = "",
        .loads = SaveNames::slot,
        .saveExt = ".dsg",
        .configFile = false,
        .deh = "",
        .bex = "",
        .exec = false,
        .map = false,
        .netgame = Netgames::none,
        .pistolStart = PistolStart::none,
        .cheats = false,
        .noAutoload = "",
        .display = Windowing::none,
        .shotdir = false,
        .teamPick = false,
        .line = PortLine::none,
};

constexpr Port ZDOOM28{
        .id = Id::zdoom,
        .loads = SaveNames::path,
        .pistolStart = PistolStart::none,
        .display = Windowing::oldCvar,
        .line = PortLine::zdoom,
};

// ZDoom 2.8 underneath, with flag words of its own and +team only once a client is in.
constexpr Port ZANDRONUM{
        .id = Id::zandronum,
        .loads = SaveNames::path,
        .pistolStart = PistolStart::none,
        .display = Windowing::oldCvar,
        .teamPick = false,
        .line = PortLine::zandronum,
};

constexpr Port UZDOOM{.id = Id::uzdoom, .netmode = false, .netgameSaves = true, .line = PortLine::uzdoom};

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

// What a dialect speaks when the program name does not name one of its ports.
Port bare(const Id id) {
    for (const Port &each :
         {ZDOOM, UZDOOM, ZDOOM28, ZANDRONUM, DSDA, BOOM, WOOF, VANILLA, ETERNITY, HELION, BOOM202, LEGACY, DOOM}) {
        if (each.id != id) {
            continue;
        }

        Port picked = each;

        // Their config is named for the port the program name gives, so the bare dialect names its own.
        if (each.id == Id::prboom || each.id == Id::woof) {
            picked.configName = choice(each.id).word;
        }

        return picked;
    }

    return {};
}

constexpr std::array WOOFS = {"woof", "nugget", "cherry"};
constexpr std::array BOOMS = {"prboom", "glboom", "rude"};

struct Version {
    std::string_view word;
    std::string_view name;
};

// Stored by place from GAME_VERSIONS on, so new ones go at the end.
constexpr std::array VERSIONS = std::to_array<Version>({
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
constexpr std::array WOOF_LEVELS = {-1, 2, 3, 4, 9, MBF, MBF21, ID24};
constexpr std::array PRBOOM_LEVELS = {-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, MBF, 12, 13, 14, 15, 16, 17};
constexpr std::array DSDA_LEVELS = {-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, MBF, 12, 13, 14, 15, 16, 17, MBF21};
constexpr std::array HELION_LEVELS = {-1, 2, 9, MBF, MBF21};

constexpr std::array<int, VERSIONS.size() + 1> VERSION_LEVELS = [] {
    std::array<int, VERSIONS.size() + 1> out{-1};

    for (size_t at = 0; at < VERSIONS.size(); ++at) {
        out[at + 1] = GAME_VERSIONS + static_cast<int>(at);
    }

    return out;
}();

const Version *versionAt(const int level) {
    const int at = level - GAME_VERSIONS;

    return at >= 0 && std::cmp_less(at, VERSIONS.size()) ? &VERSIONS[static_cast<size_t>(at)] : nullptr;
}

// Heretic, Hexen and Strife come in the same packages, with game versions and switches of their own.
bool otherGame(const std::string &name) {
    return name.contains("heretic") || name.contains("hexen") || name.contains("strife");
}

Port withoutDoomRules(Port speaks) {
    speaks.complevel = Complevels::none;
    speaks.line = PortLine::none;
    speaks.deathmatch3 = {};

    return speaks;
}

// DOS releases, matched by whole name.
constexpr std::array PLAIN = {"doom", "doom2", "doomu", "doom95", "dosdoom"};
constexpr std::array LEGACIES = {"doom3", "legacy", "doomlegacy"};

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

bool flatpakExport(const std::filesystem::path &program) {
    const std::filesystem::path folder = program.parent_path();

    return folder.filename() == "bin" && folder.parent_path().filename() == "exports"
           && folder.parent_path().parent_path().filename() == "flatpak";
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

    if (wanted == nullptr || (guessed.recognised && guessed.id == wanted->id)) {
        return guessed;
    }

    return bare(wanted->id);
}

Port of(const std::filesystem::path &program) {
    const bool exported = flatpakExport(program);

    // An export is named for its app ID, such as io.github.fabiangreffrath.woof, where only
    // the last part names the port. No DOS release comes as one.
    const std::string name = Text::lower(exported && program.has_extension() ? program.extension().string().substr(1)
                                                                             : program.stem().string());

    if (!exported && std::ranges::find(PLAIN, name) != PLAIN.end()) {
        return DOOM;
    }

    if (!exported && std::ranges::find(LEGACIES, name) != LEGACIES.end()) {
        return LEGACY;
    }

    if (name.contains("helion")) {
        return HELION;
    }

    // Whole name: gzdoom contains zdoom.
    if (name == "zdoom") {
        return ZDOOM28;
    }

    if (name.contains("zandronum") || name.contains("skulltag")) {
        return ZANDRONUM;
    }

    if (name.contains("uzdoom")) {
        return UZDOOM;
    }

    if (name.contains("zdoom") || name.contains("vkdoom")) {
        return ZDOOM;
    }

    // Chocolate plus -levelstat, which it has written since 5.9.0. It merges every -file on its own.
    if (name.contains("crispy")) {
        Port crispy = VANILLA;
        crispy.line = PortLine::crispy;
        crispy.deathmatch3 = DEATHMATCH3;
        crispy.configName = "crispy";
        crispy.pwads = "-file";
        crispy.levelstat = true;
        crispy.pistolStart = PistolStart::flag;

        return otherGame(name) ? withoutDoomRules(crispy) : crispy;
    }

    if (name.contains("chocolate")) {
        return otherGame(name) ? withoutDoomRules(VANILLA) : VANILLA;
    }

    // Whole name: prboom contains boom. MBF and WinMBF are the same line, predating
    // -complevel and everything after it.
    if (name == "boom" || name.contains("mbf")) {
        Port boom = BOOM202;

        // MBF takes -config. Boom 2.02 keeps boom.cfg beside the exe.
        boom.configFile = name.contains("mbf");

        return boom;
    }

    if (name.contains("eternity")) {
        return ETERNITY;
    }

    // Nugget and Cherry write settings that Woof would drop.
    for (const char *each : WOOFS) {
        if (name.contains(each)) {
            Port woof = WOOF;
            woof.configName = each;

            return woof;
        }
    }

    if (name.contains("dsda")) {
        return DSDA;
    }

    // A DSDA-Doom fork with its own config.
    if (name.contains("nyan")) {
        Port nyan = DSDA;
        nyan.configName = "nyan";
        nyan.line = PortLine::nyan;

        return nyan;
    }

    for (const char *each : BOOMS) {
        if (name.contains(each)) {
            Port boom = BOOM;
            boom.configName = each;

            return boom;
        }
    }

    Port guessed = ZDOOM;
    guessed.recognised = false;

    return guessed;
}

std::span<const int> complevels(const Complevels which) {
    switch (which) {
        case Complevels::gameVersion:
            return VERSION_LEVELS;
        case Complevels::woof:
            return WOOF_LEVELS;
        case Complevels::prboom:
            return PRBOOM_LEVELS;
        case Complevels::dsda:
            return DSDA_LEVELS;
        case Complevels::helion:
            return HELION_LEVELS;
        case Complevels::none:
            break;
    }

    return {};
}

std::string complevelWord(const Complevels which, const int level) {
    if (level < 0) {
        return {};
    }

    if (which == Complevels::gameVersion) {
        const Version *version = versionAt(level);

        return version == nullptr ? std::string() : std::string(version->word);
    }

    if (which == Complevels::helion) {
        switch (level) {
            case 2:
                return "vanilla";
            case 9:
                return "boom";
            case 11:
                return "mbf";
            case 21:
                return "mbf21";
            default:
                return {};
        }
    }

    return std::to_string(level);
}

std::string_view complevelSwitch(const Complevels which) {
    switch (which) {
        case Complevels::helion:
            return "+complevel";
        case Complevels::gameVersion:
            return "-gameversion";
        default:
            return "-complevel";
    }
}

std::string_view versionName(const int level) {
    const Version *version = versionAt(level);

    return version == nullptr ? std::string_view() : version->name;
}

DemoSupport demos(const Port &speaks) {
    return {
            .records = speaks.demos,
            .continues = speaks.demos && speaks.continues,
            .timed = speaks.demos && speaks.timedemo,
            .fast = speaks.demos && speaks.fastdemo,
            .complevel = speaks.demos ? speaks.complevel : Complevels::none,
            .longtics = speaks.demos && speaks.longtics,
            .soloNet = speaks.demos && speaks.soloNet,
    };
}

SaveSupport saves(const Port &speaks) {
    return {.names = speaks.loads, .folder = !speaks.save.empty()};
}

NetSupport net(const Port &speaks) {
    switch (speaks.netgame) {
        case Netgames::zdoom:
            return {
                    .hosts = true,
                    .joins = true,
                    .players = true,
                    .fragLimit = true,
                    .timeLimitCvar = true,
                    .savegame = true,
                    .netgameFolder = speaks.netgameSaves,
                    .extratic = true,
                    .netmode = speaks.netmode,
                    .dup = true,
                    .player = true,
                    .teams = true,
                    .teamPick = speaks.teamPick,
            };
        case Netgames::chocolate:
            return {
                    .hosts = true,
                    .joins = true,
                    .listing = true,
                    .dup = true,
                    .deathmatch3 = !speaks.deathmatch3.empty(),
            };

        case Netgames::prboom:
            return {.joins = true};
        case Netgames::none:
            break;
    }

    return {};
}

Port of(const Config &config) {
    return of(config, config.activeProfile());
}

Port of(const Config &config, const Profile &profile) {
    const PortEntry *entry = config.findPort(profile.port);

    return entry == nullptr ? Port{} : of(*entry);
}

namespace {

Port spoken(const PortEntry &entry, const std::string_view picked) {
    Port speaks = of(std::filesystem::path(entry.file), picked);
    speaks.dos = entry.dosbox;

    if (!speaks.dos) {
        return speaks;
    }

    // Only the DOS dialects ever ran there, so any other name is answered for as vanilla.
    if (!speaks.recognised || !choice(speaks.id).dos) {
        speaks = DOOM;
        speaks.dos = true;
        speaks.recognised = false;
    }

    // Nothing after the last DOS release applies inside DOSBox.
    speaks.save = "";
    speaks.extraConfig = false;
    speaks.exec = false;
    speaks.map = false;
    speaks.netgame = Netgames::none;
    speaks.complevel = Complevels::none;
    speaks.longtics = false;
    speaks.soloNet = false;
    speaks.levelstat = false;
    speaks.pistolStart = PistolStart::none;
    speaks.cheats = false;
    speaks.shotdir = false;
    speaks.continues = false;
    speaks.recordsWithComplevel = false;
    speaks.line = PortLine::none;

    return speaks;
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
        Port speaks;
    };

    // One push asks for the same port many times over. Every view in a Port names a literal.
    // Assigned in place, so a miss reuses the room the strings already have.
    thread_local Asked last;

    if (!last.known || last.dosbox != entry.dosbox || last.file != entry.file || last.picked != picked) {
        last.known = true;
        last.file = entry.file;
        last.picked = picked;
        last.dosbox = entry.dosbox;
        last.speaks = spoken(entry, picked);
    }

    return last.speaks;
}

}
