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
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <optional>
#include <ranges>
#include <span>
#include <utility>

#include "ttk/system/Text.h"

#include "core/config/Schema.h"
#include "core/launch/Arguments.h"
#include "core/launch/Dialect.h"
#include "core/launch/Flags.h"
#include "core/launch/Launcher.h"
#include "core/launch/Netgame.h"
#include "core/launch/Rules.h"
#include "core/launch/Storage.h"
#include "core/wad/MapFile.h"

using namespace ttk;

namespace Arguments {

namespace {

#ifdef _WIN32
constexpr bool WINDOWS = true;
#else
constexpr bool WINDOWS = false;
#endif

struct ClassifiedFiles {
    std::vector<std::string> pwads;
    std::vector<std::string> dehs;
    std::vector<std::string> bexs;
    std::vector<std::string> autoexecs;
    std::vector<std::string> lumps;

    // The kind that appeared last in the list is applied last.
    bool dehLast{true};
};

// A name that answers to nothing here is left for the port's own search path.
std::string beside(const std::string &file, const std::filesystem::path &directory) {
    if (directory.empty() || file.empty() || std::filesystem::path(file).is_absolute()) {
        return file;
    }

    std::error_code code;
    const std::filesystem::path found = directory / file;

    return std::filesystem::exists(found, code) ? found.string() : file;
}

ClassifiedFiles classifyFiles(const std::vector<FileEntry> &files, const std::filesystem::path &directory) {
    ClassifiedFiles out;

    for (const FileEntry &entry : files) {
        // Most ports stop at startup on a file they cannot open.
        if (!entry.enabled || entry.gone) {
            continue;
        }

        if (Text::iends_with(entry.file, ".bex")) {
            out.dehLast = false;
            out.bexs.push_back(beside(entry.file, directory));
        } else if (Text::iends_with(entry.file, ".deh") || Text::iends_with(entry.file, ".hhe")
                   || Text::iends_with(entry.file, ".seh")) {
            out.dehLast = true;
            out.dehs.push_back(beside(entry.file, directory));
        } else if (Text::iends_with(entry.file, ConfigFile::CFG_EXT)) {
            out.autoexecs.push_back(beside(entry.file, directory));
        } else if (Text::iends_with(entry.file, ".lmp")) {
            out.lumps.push_back(beside(entry.file, directory));
        } else {
            out.pwads.push_back(beside(entry.file, directory));
        }
    }

    return out;
}

bool digit(const char letter) {
    return letter >= '0' && letter <= '9';
}

bool letterIs(const char letter, const char wanted) {
    return std::tolower(static_cast<unsigned char>(letter)) == wanted;
}

// What a picked map asks of the files, read before any argument is written.
struct Warp {
    // Kept on the command line.
    bool offered{false};
    // An iwad is named, so the map can go as -warp numbers.
    bool numbers{false};
    bool mapxx{false};
};

std::vector<std::string> warpArguments(const Warp &warp, const std::string &map, const Dialect::Port &speaks) {
    if (!warp.numbers) {
        return {};
    }

    if (warp.mapxx) {
        // MAPxx
        if (map.size() == 5 && letterIs(map[0], 'm') && letterIs(map[1], 'a') && letterIs(map[2], 'p') && digit(map[3])
            && digit(map[4])) {
            return {"-warp", map.substr(3, 2)};
        }

        return {};
    }

    // ExMy, and ExMyy for the ports that take it
    if ((map.size() == 4 || (map.size() == 5 && speaks.longWarp)) && letterIs(map[0], 'e') && digit(map[1])
        && letterIs(map[2], 'm') && map[3] >= '1' && map[3] <= '9' && (map.size() == 4 || digit(map[4]))) {
        return {"-warp", map.substr(1, 1), map.substr(3)};
    }

    return {};
}

bool typesFiles(const std::vector<std::string> &arguments) {
    return std::ranges::any_of(arguments, [](const std::string &argument) {
        return Text::iequals(argument, "-file") || Text::iequals(argument, "-merge");
    });
}

// A map whose add-on is now off or gone stops most ports at startup. The choice is
// trusted unless every file that could hold maps was read and none of them does.
Warp warpOf(const std::string &map,
            const std::string &iwad,
            const std::vector<std::string> &pwads,
            const std::vector<std::string> &always,
            const std::vector<std::string> &extra) {
    if (map.empty()) {
        return {};
    }

    if (iwad.empty()) {
        return Warp{.offered = true, .numbers = false, .mapxx = false};
    }

    // An unread file may hold the map, which settles the answer without reading the rest.
    const auto lists = [&map](const MapFile::Maps &maps) {
        return !maps.opened
               || std::ranges::any_of(maps.names, [&map](const std::string &name) { return Text::iequals(name, map); });
    };

    // Taken before the add-ons are read, which may drop the iwad from the cache.
    const MapFile::Maps &game = MapFile::maps(iwad);
    const bool mapxx = game.mapxx;
    const bool listed = lists(game);

    const auto pwadLists = [&lists](const std::string &file) { return lists(MapFile::maps(file)); };
    const bool offered = listed || std::ranges::any_of(pwads, pwadLists) || typesFiles(always) || typesFiles(extra);

    return Warp{.offered = offered, .numbers = true, .mapxx = mapxx};
}

void append(std::vector<std::string> &into, const std::vector<std::string> &what) {
    into.insert(into.end(), what.begin(), what.end());
}

void append(std::vector<std::string> &into, std::vector<std::string> &&what) {
    into.insert(into.end(), std::make_move_iterator(what.begin()), std::make_move_iterator(what.end()));
}

void addSettings(std::vector<std::string> &args, const Config &config, const Dialect::Port &speaks) {
    if (const std::filesystem::path own = Storage::configFile(config); !own.empty()) {
        // Legacy would write over the file without reading it, and that file may be another port's.
        if (speaks.configFile) {
            args.emplace_back("-config");
            args.push_back(own.string());
        }

        if (speaks.extraConfig) {
            args.emplace_back("-extraconfig");
            args.push_back(Storage::extraConfigFile(own).string());
        }

        if (const std::filesystem::path saves = Storage::saveDirectory(config);
            !saves.empty() && !speaks.save.empty()) {
            args.emplace_back(speaks.save);
            args.push_back(saves.string());
        }
    }
}

void addGame(std::vector<std::string> &args,
             const Profile &profile,
             const Dialect::Port &speaks,
             const std::string &iwad,
             const Warp &warp) {
    if (!iwad.empty() && speaks.iwad) {
        args.emplace_back("-iwad");
        args.push_back(iwad);
    }

    if (profile.monsters > 0) {
        if (profile.monsters == 1) {
            args.emplace_back("-nomonsters");
        } else {
            if (profile.monsters % 2 == 0) {
                args.emplace_back("-fast");
            }

            if (profile.monsters >= 3 && speaks.respawn) {
                args.emplace_back("-respawn");
            }
        }
    }

    if (profile.skill > 0) {
        args.emplace_back("-skill");
        args.push_back(std::to_string(profile.skill));
    }

    if (warp.offered) {
        if (std::vector<std::string> numbers = warpArguments(warp, profile.warp, speaks); !numbers.empty()) {
            append(args, std::move(numbers));
        } else if (speaks.map) {
            args.emplace_back("+map");
            args.push_back(profile.warp);
        }
    }
}

void addFiles(std::vector<std::string> &args,
              ClassifiedFiles files,
              const Dialect::Port &speaks,
              const std::filesystem::path &demo) {
    // A port only plays the first demo it is handed, so the profile's own wins.
    if (!demo.empty()) {
        files.lumps.clear();
    }

    if (speaks.bex.empty()) {
        files.bexs.clear();
    }

    if (speaks.deh.empty()) {
        files.dehs.clear();
    }

    if (!speaks.exec) {
        files.autoexecs.clear();
    }

    if (!files.pwads.empty()) {
        args.emplace_back(speaks.pwads);
        append(args, std::move(files.pwads));
    }

    // Each switch appears once with every file behind it. Ports stop at the first -deh.
    const bool bexFirst = files.dehLast;
    const std::vector<std::string> &first = bexFirst ? files.bexs : files.dehs;
    const std::vector<std::string> &second = bexFirst ? files.dehs : files.bexs;

    if (!first.empty()) {
        args.emplace_back(bexFirst ? speaks.bex : speaks.deh);
        append(args, first);
    }

    if (!second.empty()) {
        if (first.empty() || speaks.deh != speaks.bex) {
            args.emplace_back(bexFirst ? speaks.deh : speaks.bex);
        }

        append(args, second);
    }

    for (const std::string &file : files.autoexecs) {
        args.emplace_back("+exec");
        args.push_back(file);
    }

    for (const std::string &file : files.lumps) {
        args.emplace_back("-playdemo");
        args.push_back(file);
    }
}

// A vanilla demo recorded with long tics says so in its first byte.
bool longticsDemo(const std::filesystem::path &demo) {
    static constexpr char LONGTICS_VERSION = 111;

    std::ifstream in(demo, std::ios::binary);
    char version = 0;

    return in.get(version) && version == LONGTICS_VERSION;
}

std::string recordedAs(const std::filesystem::path &demo, const Dialect::Port &speaks) {
    return (speaks.recordAddsLmp ? demo.parent_path() / demo.stem() : demo).string();
}

void addComplevel(std::vector<std::string> &args, const Dialect::Complevels which, const int level) {
    args.emplace_back(Dialect::complevelSwitch(which));
    args.push_back(Dialect::complevelWord(which, level));
}

enum class DemoUse : std::uint8_t {
    none,
    records,
    plays,
    continues,
};

// A demo among the add-ons is dropped for one the profile records or continues.
DemoUse demoUse(const Profile &profile, const std::filesystem::path &demo, const bool playsDemos) {
    if (demo.empty()) {
        return playsDemos ? DemoUse::plays : DemoUse::none;
    }

    switch (profile.replay.mode) {
        case ReplayMode::Record:
            return DemoUse::records;
        case ReplayMode::Continue:
            return DemoUse::continues;
        case ReplayMode::Play:
            return DemoUse::plays;
        case ReplayMode::Off:
            break;
    }

    return DemoUse::none;
}

// Before -record, since the level decides the demo header. A demo played back carries its complevel, but not the
// game version: Chocolate stops on a demo from another one. Returns the level put on the line, or -1.
int addCompatibility(std::vector<std::string> &args,
                     const Profile &profile,
                     const Dialect::Port &speaks,
                     const DemoUse use) {
    const std::span<const int> reads = Dialect::complevels(speaks.complevel);
    const auto picked = std::ranges::find(reads, profile.complevel);
    const bool chosen = profile.complevel >= 0 && picked != reads.end();

    if (use == DemoUse::continues) {
        // The new demo takes its header from the old one, so any level the port reads will do.
        const auto any = std::ranges::find_if(reads, [](const int level) { return level >= 0; });

        if (const auto level = chosen ? picked : any; speaks.continueComplevel && level != reads.end()) {
            addComplevel(args, speaks.complevel, *level);

            return *level;
        }

        return -1;
    }

    if (chosen && (use != DemoUse::plays || speaks.complevel == Dialect::Complevels::gameVersion)) {
        addComplevel(args, speaks.complevel, profile.complevel);

        return profile.complevel;
    }

    return -1;
}

// A joining player plays by the rules of the host.
void addFlags(std::vector<std::string> &args, const Profile &profile, const Dialect::Port &speaks) {
    if (speaks.line == PortLine::none || Netgame::joins(profile, speaks)) {
        return;
    }

    for (const Flags::Word word : Flags::ALL) {
        if (!Flags::reads(speaks.line, word)) {
            continue;
        }

        if (const std::optional<std::uint32_t> value = Flags::parse(profile.flag(word))) {
            args.push_back(Flags::switchFor(speaks.line, word));
            args.push_back(Flags::decimal(speaks.line, *value));
        }
    }
}

// Left out where the port would stop on them. A joining player plays by the rules of the host.
void addRules(std::vector<std::string> &args,
              const Profile &profile,
              const Dialect::Port &speaks,
              const DemoUse use,
              const int level) {
    if (speaks.line == PortLine::none || Netgame::joins(profile, speaks)) {
        return;
    }

    std::uint32_t now = Netgame::inPlay(profile, speaks) ? Rules::NETGAME : 0U;

    if (use == DemoUse::plays) {
        now |= Rules::PLAYBACK;
    } else if (use != DemoUse::none) {
        now |= Rules::RECORDING;
    }

    for (const Rules::Rule rule : Rules::ALL) {
        const Rules::Spelling *spelled = Rules::spelling(speaks.line, rule);

        if (spelled == nullptr || Dialect::lacksRule(speaks, rule) || (spelled->refused & now) != 0
            || !Rules::worksAt(*spelled, level)) {
            continue;
        }

        if (Rules::info(rule).kind == Rules::Kind::toggle) {
            if (profile.ruleOn(rule)) {
                args.emplace_back(spelled->name);

                if (!spelled->value.empty()) {
                    args.emplace_back(spelled->value);
                }
            }
        } else if (const std::optional<int> value = Rules::parse(*spelled, profile.rule(rule))) {
            args.emplace_back(spelled->name);
            args.push_back(std::to_string(*value));
        }
    }
}

void addContinue(std::vector<std::string> &args,
                 const Config &config,
                 const Dialect::Port &speaks,
                 const std::filesystem::path &demo) {
    const std::filesystem::path from = Storage::replaySource(config);

    if (from.empty()) {
        return;
    }

    // Playback reads the old demo through the same setting, so it has to match.
    if (speaks.longtics && longticsDemo(from)) {
        args.emplace_back("-longtics");
    }

    args.emplace_back("-recordfromto");
    args.push_back(from.string());
    args.push_back(recordedAs(demo, speaks));
}

void addDemo(std::vector<std::string> &args,
             const Config &config,
             const Dialect::Port &speaks,
             const std::filesystem::path &demo) {
    const ReplaySettings &replay = config.activeProfile().replay;

    if (!demo.empty()) {
        if (replay.mode == ReplayMode::Continue) {
            addContinue(args, config, speaks, demo);
        } else if (replay.mode == ReplayMode::Record) {
            // These must precede -record. They decide the demo header.
            if (speaks.longtics && replay.longtics) {
                args.emplace_back("-longtics");
            }

            if (speaks.soloNet && replay.soloNet) {
                args.emplace_back("-solo-net");
            }

            args.emplace_back("-record");
            args.push_back(recordedAs(demo, speaks));
        } else {
            std::string_view how = "-playdemo";

            if (replay.playback == Playback::Timed && speaks.timedemo) {
                how = "-timedemo";
            } else if (replay.playback == Playback::Fast && speaks.fastdemo) {
                how = "-fastdemo";
            }

            args.emplace_back(how);
            args.push_back(demo.string());
        }
    }
}

void addDisplay(std::vector<std::string> &args, const Display display, const Dialect::Windowing windowing) {
    if (display == Display::Port) {
        return;
    }

    const bool full = display == Display::Fullscreen;

    switch (windowing) {
        case Dialect::Windowing::none:
            break;

        case Dialect::Windowing::switches:
            args.emplace_back(full ? "-fullscreen" : "-window");
            break;

        case Dialect::Windowing::noWindow:
            args.emplace_back(full ? "-nowindow" : "-window");
            break;

        case Dialect::Windowing::cvar:
        case Dialect::Windowing::oldCvar:
            args.emplace_back(windowing == Dialect::Windowing::cvar ? "+vid_fullscreen" : "+fullscreen");
            args.emplace_back(full ? "1" : "0");
            break;
    }
}

void addGameplay(std::vector<std::string> &args, const Config &config, const Dialect::Port &speaks, const DemoUse use) {
    const Profile &profile = config.activeProfile();

    // The ports with the switch stop on it while a demo records, and some in a netgame too.
    if (profile.on(Switch::pistolStart) && speaks.pistolStart == Dialect::PistolStart::cvar) {
        args.emplace_back("+sv_pistolstart");
        args.emplace_back("1");
    } else if (profile.on(Switch::pistolStart) && speaks.pistolStart == Dialect::PistolStart::flag
               && use == DemoUse::none && !Netgame::inPlay(profile, speaks)) {
        args.emplace_back("-pistolstart");
    }

    if (speaks.cheats && profile.on(Switch::cheats)) {
        args.emplace_back("+sv_cheats");
        args.emplace_back("1");
    }

    if (!speaks.noAutoload.empty() && profile.on(Switch::skipAutoload)) {
        args.emplace_back(speaks.noAutoload);
    }

    if (speaks.nomusic && !profile.on(Switch::music)) {
        args.emplace_back("-nomusic");
    }

    if (!speaks.nosfx.empty() && !profile.on(Switch::soundEffects)) {
        args.emplace_back(speaks.nosfx);
    }

    addDisplay(args, profile.display, speaks.display);

    if (const std::filesystem::path shots = Storage::screenshotDirectory(config); speaks.shotdir && !shots.empty()) {
        args.emplace_back("-shotdir");
        args.push_back(shots.string());
    }
}

std::filesystem::path addSave(std::vector<std::string> &args,
                              const Config &config,
                              const Dialect::Port &speaks,
                              const std::filesystem::path &demo) {
    const Profile &profile = config.activeProfile();

    // The profile save sits outside the netgame folder, so the port would stop on it.
    const bool unreachable = Netgame::readsNetgameFolder(profile, speaks);

    std::filesystem::path save =
            profile.save.enabled && demo.empty() && !unreachable ? Storage::saveFile(config) : std::filesystem::path();

    // The port stops on a save it cannot find, so the line goes without one.
    if (std::error_code code; !save.empty() && !std::filesystem::is_regular_file(save, code)) {
        save.clear();
    }

    if (!save.empty() && speaks.loads == Dialect::SaveNames::slot
        && Storage::saveSlot(save.filename().string()) > speaks.lastSlot) {
        save.clear();
    }

    if (std::string named = save.empty() ? std::string() : Storage::loadName(config, save, speaks); !named.empty()) {
        args.emplace_back("-loadgame");
        args.push_back(std::move(named));
    }

    return save;
}

std::vector<std::string>
build(const Config &config, const std::filesystem::path &portDirectory, const bool mayCapture) {
    std::vector<std::string> args;
    const Profile &profile = config.activeProfile();
    const std::string iwad = beside(config.activeIwadFile(), portDirectory);
    const Dialect::Port speaks = Dialect::of(config);

    const ReplayMode mode = profile.replay.mode;
    const bool replays =
            mode != ReplayMode::Off && speaks.demos
            && (mode != ReplayMode::Continue || (speaks.continues && !Storage::replaySource(config).empty()));
    const std::filesystem::path demo = replays ? Storage::replayFile(config) : std::filesystem::path();

    ClassifiedFiles files = classifyFiles(profile.files, portDirectory);
    const DemoUse use = demoUse(profile, demo, !files.lumps.empty());
    std::vector<std::string> always = Text::parse_arguments(config.general.alwaysAdd);
    std::vector<std::string> extra = Text::parse_arguments(profile.extra);

    addSettings(args, config, speaks);
    addGame(args, profile, speaks, iwad, warpOf(profile.warp, iwad, files.pwads, always, extra));
    addFiles(args, std::move(files), speaks, demo);
    const int level = addCompatibility(args, profile, speaks, use);
    addDemo(args, config, speaks, demo);

    // Ahead of +sv_pistolstart, which a later +dmflags3 would clear.
    addFlags(args, profile, speaks);

    if (speaks.levelstat && profile.on(Switch::levelstat)) {
        args.emplace_back("-levelstat");
    }

    addGameplay(args, config, speaks, use);
    addRules(args, profile, speaks, use, level);

    const std::filesystem::path save = addSave(args, config, speaks, demo);

    Netgame::arguments(args, profile, speaks, save);

    if (mayCapture && asksForStdout(config, speaks, WINDOWS)) {
        args.emplace_back("-stdout");
    }

    append(args, std::move(always));
    append(args, std::move(extra));

    return args;
}

}

std::vector<std::string> of(const Config &config, const std::filesystem::path &portDirectory) {
    return build(config, portDirectory, true);
}

std::vector<std::string> uncaptured(const Config &config, const std::filesystem::path &portDirectory) {
    return build(config, portDirectory, false);
}

std::vector<std::string> maps(const Config &config) {
    std::vector<std::string> names;
    const Profile &profile = config.activeProfile();

    if (const std::string iwad = config.activeIwadFile(); !iwad.empty()) {
        append(names, MapFile::maps(iwad).names);
    }

    for (const FileEntry &entry : profile.files) {
        if (!entry.enabled) {
            continue;
        }

        append(names, MapFile::maps(entry.file).names);
    }

    std::ranges::sort(names, Text::natural_less);
    names.erase(std::ranges::unique(names).begin(), names.end());

    return names;
}

bool asksForStdout(const Config &config, const bool windows) {
    return windows && asksForStdout(config, Dialect::of(config), true);
}

bool asksForStdout(const Config &config, const Dialect::Port &speaks, const bool windows) {
    return windows && Launcher::captures(config, speaks) && speaks.recognised
           && (speaks.id == Dialect::Id::gzdoom || speaks.id == Dialect::Id::zdoom
               || speaks.id == Dialect::Id::zandronum);
}

}
