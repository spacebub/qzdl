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

#include "core/config/Options.h"
#include "core/config/Schema.h"
#include "core/launch/Arguments.h"
#include "core/launch/Dialect.h"
#include "core/launch/Flags.h"
#include "core/launch/Launcher.h"
#include "core/launch/Netgame.h"
#include "core/launch/Storage.h"
#include "core/wad/MapFile.h"

using namespace ttk;

namespace Arguments {

using Dialect::Feature;

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

    bool firstPatchIsBex{false};
};

ClassifiedFiles classifyFiles(const std::vector<FileEntry> &files, const std::filesystem::path &directory) {
    ClassifiedFiles out;

    for (const FileEntry &entry : files) {
        // Most ports stop at startup on a file they cannot open.
        if (!entry.enabled || entry.gone) {
            continue;
        }

        if (Text::iends_with(entry.file, ".bex")) {
            if (out.dehs.empty() && out.bexs.empty()) {
                out.firstPatchIsBex = true;
            }

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

std::vector<std::string> warpArguments(const Warp &warp, const std::string &map, const Dialect::Traits &traits) {
    if (!warp.numbers) {
        return {};
    }

    if (warp.mapxx) {
        // MAPxx
        if (map.size() == 5 && letterIs(map[0], 'm') && letterIs(map[1], 'a') && letterIs(map[2], 'p') && digit(map[3])
            && digit(map[4])) {
            return {map.substr(3, 2)};
        }

        return {};
    }

    // ExMy, and ExMyy for the ports that take it
    if ((map.size() == 4 || (map.size() == 5 && traits.has(Feature::longWarp))) && letterIs(map[0], 'e')
        && digit(map[1]) && letterIs(map[2], 'm') && map[3] >= '1' && map[3] <= '9'
        && (map.size() == 4 || digit(map[4]))) {
        return {map.substr(1, 1), map.substr(3)};
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

void reads(std::vector<Reached> *reached, const std::filesystem::path &path) {
    if (reached != nullptr && path.is_absolute()) {
        reached->push_back({.path = path, .access = Access::reads});
    }
}

void reads(std::vector<Reached> *reached, const std::string &path) {
    if (reached != nullptr) {
        reads(reached, std::filesystem::path(path));
    }
}

void reads(std::vector<Reached> *reached, const std::vector<std::string> &paths) {
    if (reached != nullptr) {
        for (const std::string &path : paths) {
            reads(reached, std::filesystem::path(path));
        }
    }
}

void writesIn(std::vector<Reached> *reached, const std::filesystem::path &folder) {
    if (reached != nullptr && folder.is_absolute()) {
        reached->push_back({.path = folder, .access = Access::writes});
    }
}

// A written file may not exist yet, and a port may replace it through a temporary beside it.
void writesBeside(std::vector<Reached> *reached, const std::filesystem::path &file) {
    if (reached != nullptr) {
        writesIn(reached, file.parent_path());
    }
}

void addSettings(std::vector<std::string> &args,
                 std::vector<Reached> *reached,
                 const Config &config,
                 const Dialect::Traits &traits) {
    if (const std::filesystem::path own = Storage::configFile(config); !own.empty()) {
        // Legacy would write over the file without reading it, and that file may be another port's.
        if (Dialect::add(args, traits, Feature::configFile, own.string())) {
            writesBeside(reached, own);
        }

        if (traits.has(Feature::extraConfig)) {
            Dialect::add(args, traits, Feature::extraConfig, Storage::extraConfigFile(own).string());
        }

        if (const std::filesystem::path saves = Storage::saveDirectory(config);
            !saves.empty() && Dialect::add(args, traits, Feature::saveFolder, saves.string())) {
            writesIn(reached, saves);
        }
    }
}

void addGame(std::vector<std::string> &args,
             std::vector<Reached> *reached,
             const Profile &profile,
             const Dialect::Traits &traits,
             const std::string &iwad,
             const Warp &warp) {
    // The launcher points DOOMWADDIR at the IWAD folder, since a port loads companions such as strife1.wad from there.
    if (!iwad.empty() && Dialect::add(args, traits, Feature::iwad, iwad) && reached != nullptr) {
        reads(reached, std::filesystem::path(iwad).parent_path());
    }

    if (profile.monsters > 0) {
        if (profile.monsters == 1) {
            Dialect::add(args, traits, Feature::noMonsters);
        } else {
            if (profile.monsters % 2 == 0) {
                Dialect::add(args, traits, Feature::fast);
            }

            if (profile.monsters >= 3) {
                Dialect::add(args, traits, Feature::respawn);
            }
        }
    }

    if (profile.skill > 0) {
        Dialect::add(args, traits, Feature::skill, std::to_string(profile.skill));
    }

    if (warp.offered) {
        if (std::vector<std::string> numbers = warpArguments(warp, profile.warp, traits);
            !numbers.empty() && Dialect::add(args, traits, Feature::warp)) {
            append(args, std::move(numbers));
        } else {
            Dialect::add(args, traits, Feature::map, profile.warp);
        }
    }
}

void addFiles(std::vector<std::string> &args,
              std::vector<Reached> *reached,
              ClassifiedFiles files,
              const Dialect::Traits &traits,
              const std::filesystem::path &demo) {
    // A port only plays the first demo it is handed, so the profile's own wins.
    if (!demo.empty()) {
        files.lumps.clear();
    }

    if (!traits.has(Feature::bex)) {
        files.bexs.clear();
    }

    if (!traits.has(Feature::deh)) {
        files.dehs.clear();
    }

    // The port loads the first patch in the list and drops the rest without a word.
    if (traits.has(Feature::onePatch)) {
        if (files.firstPatchIsBex && !files.bexs.empty()) {
            files.bexs.resize(1);
            files.dehs.clear();
        } else if (!files.dehs.empty()) {
            files.dehs.resize(1);
            files.bexs.clear();
        } else {
            files.bexs.resize(std::min<std::size_t>(files.bexs.size(), 1));
        }
    }

    if (!files.pwads.empty() && Dialect::add(args, traits, Feature::pwads)) {
        reads(reached, files.pwads);
        append(args, std::move(files.pwads));
    }

    // Each argument appears once with every file behind it. Ports stop at the first -deh.
    const bool bexFirst = files.dehLast;
    const std::vector<std::string> &first = bexFirst ? files.bexs : files.dehs;
    const std::vector<std::string> &second = bexFirst ? files.dehs : files.bexs;

    if (!first.empty()) {
        Dialect::add(args, traits, bexFirst ? Feature::bex : Feature::deh);
        reads(reached, first);
        append(args, first);
    }

    if (!second.empty()) {
        if (first.empty() || Dialect::arg(traits, Feature::deh).name != Dialect::arg(traits, Feature::bex).name) {
            Dialect::add(args, traits, bexFirst ? Feature::deh : Feature::bex);
        }

        reads(reached, second);
        append(args, second);
    }

    for (const std::string &file : files.autoexecs) {
        if (Dialect::add(args, traits, Feature::exec, file)) {
            reads(reached, file);
        }
    }

    for (const std::string &file : files.lumps) {
        if (Dialect::add(args, traits, Feature::playdemo, file)) {
            reads(reached, file);
        }
    }
}

// A vanilla demo recorded with long tics says so in its first byte.
bool longticsDemo(const std::filesystem::path &demo) {
    static constexpr char LONGTICS_VERSION = 111;

    std::ifstream in(demo, std::ios::binary);
    char version = 0;

    return in.get(version) && version == LONGTICS_VERSION;
}

std::string recordedAs(const std::filesystem::path &demo, const Dialect::Traits &traits) {
    return (traits.has(Feature::recordAddsLmp) ? demo.parent_path() / demo.stem() : demo).string();
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
                     const Dialect::Traits &traits,
                     const DemoUse use) {
    const int chosen = Dialect::pickedLevel(traits.complevel, profile.complevel);

    if (use == DemoUse::continues) {
        // The new demo takes its header from the old one, so any level the port reads will do.
        const std::span<const int> reads = Dialect::complevels(traits.complevel);
        const auto any = std::ranges::find_if(reads, [](const int level) { return level >= 0; });
        const int level = chosen >= 0 || any == reads.end() ? chosen : *any;

        if (traits.has(Feature::continueComplevel) && level >= 0) {
            Dialect::add(args, traits, Feature::complevel, Dialect::complevelWord(traits.complevel, level));

            return level;
        }

        return -1;
    }

    if (chosen >= 0 && (use != DemoUse::plays || Dialect::compatibility(traits) == Dialect::Compatibility::versions)) {
        Dialect::add(args, traits, Feature::complevel, Dialect::complevelWord(traits.complevel, chosen));

        return chosen;
    }

    return -1;
}

// A joining player plays by the rules of the host.
void addFlags(std::vector<std::string> &args, const Profile &profile, const Dialect::Traits &traits) {
    if (Netgame::joins(profile, traits)) {
        return;
    }

    for (const Flags::Word word : Flags::ALL) {
        if (const std::optional<std::uint32_t> value = Flags::parse(profile.flag(word))) {
            Dialect::add(args, traits, Flags::feature(word), Flags::decimal(*value, traits.has(Feature::signedWords)));
        }
    }
}

// Left out where the port would stop on them. A joining player plays by the rules of the host.
void addOptions(std::vector<std::string> &args,
                const Profile &profile,
                const Dialect::Traits &traits,
                const DemoUse use,
                const int level) {
    const bool joins = Netgame::joins(profile, traits);
    std::uint32_t now = Netgame::inPlay(profile, traits) ? Dialect::NETGAME : 0U;

    if (use == DemoUse::plays) {
        now |= Dialect::PLAYBACK;
    } else if (use != DemoUse::none) {
        now |= Dialect::RECORDING;
    }

    for (const Options::Option &option : Options::ALL) {
        const bool fromHost = joins && option.group == Options::Group::rules;

        if (fromHost || !traits.has(option.feature) || profile.stored(option).empty()) {
            continue;
        }

        const Dialect::Arg &arg = Dialect::arg(traits, option.feature);

        if ((arg.refused & now) != 0 || !Dialect::worksAt(arg, level)) {
            continue;
        }

        if (option.kind == Options::Kind::toggle) {
            Dialect::add(args, traits, option.feature);
        } else if (const std::optional<int> value = Dialect::parse(arg, profile.stored(option))) {
            Dialect::add(args, traits, option.feature, std::to_string(*value));
        }
    }
}

void addContinue(std::vector<std::string> &args,
                 std::vector<Reached> *reached,
                 const Dialect::Traits &traits,
                 const std::filesystem::path &demo,
                 const std::filesystem::path &from) {
    // Playback reads the old demo through the same setting, so it has to match.
    if (longticsDemo(from)) {
        Dialect::add(args, traits, Feature::longtics);
    }

    Dialect::add(args, traits, Feature::continues, from.string());
    args.push_back(recordedAs(demo, traits));
    reads(reached, from);
    writesBeside(reached, demo);
}

void addDemo(std::vector<std::string> &args,
             std::vector<Reached> *reached,
             const ReplaySettings &replay,
             const Dialect::Traits &traits,
             const std::filesystem::path &demo,
             const std::filesystem::path &source) {
    if (!demo.empty()) {
        if (replay.mode == ReplayMode::Continue) {
            addContinue(args, reached, traits, demo, source);
        } else if (replay.mode == ReplayMode::Record) {
            // These must precede -record. They decide the demo header.
            if (replay.longtics) {
                Dialect::add(args, traits, Feature::longtics);
            }

            if (replay.soloNet) {
                Dialect::add(args, traits, Feature::soloNet);
            }

            if (Dialect::add(args, traits, Feature::record, recordedAs(demo, traits))) {
                writesBeside(reached, demo);
            }
        } else {
            Dialect::Feature how = Feature::playdemo;

            if (replay.playback == Playback::Timed && traits.has(Feature::timedemo)) {
                how = Feature::timedemo;
            } else if (replay.playback == Playback::Fast && traits.has(Feature::fastdemo)) {
                how = Feature::fastdemo;
            }

            if (Dialect::add(args, traits, how, demo.string())) {
                reads(reached, demo);
            }
        }
    }
}

void addScreen(std::vector<std::string> &args,
               std::vector<Reached> *reached,
               const Config &config,
               const Dialect::Traits &traits) {
    const Profile &profile = config.activeProfile();

    if (profile.display != Display::Port) {
        Dialect::add(args, traits, profile.display == Display::Fullscreen ? Feature::fullscreen : Feature::windowed);
    }

    if (const std::filesystem::path shots = Storage::screenshotDirectory(config);
        !shots.empty() && Dialect::add(args, traits, Feature::shotdir, shots.string())) {
        writesIn(reached, shots);
    }
}

std::filesystem::path addSave(std::vector<std::string> &args,
                              std::vector<Reached> *reached,
                              const Config &config,
                              const Dialect::Traits &traits,
                              const std::filesystem::path &demo) {
    const Profile &profile = config.activeProfile();

    // The profile save sits outside the netgame folder, so the port would stop on it.
    const bool unreachable = Netgame::readsNetgameFolder(profile, traits);

    std::filesystem::path save =
            profile.save.enabled && demo.empty() && !unreachable ? Storage::saveFile(config) : std::filesystem::path();

    // The port stops on a save it cannot find, so the line goes without one.
    if (std::error_code code; !save.empty() && !std::filesystem::is_regular_file(save, code)) {
        save.clear();
    }

    if (!save.empty() && Storage::pastLastSlot(traits, save.filename().string())) {
        save.clear();
    }

    if (std::string named = save.empty() ? std::string() : Storage::loadName(config, save, traits); !named.empty()) {
        reads(reached, named);
        Dialect::add(args, traits, Feature::loadgame, std::move(named));
    }

    return save;
}

std::vector<std::string> build(const Config &config,
                               const std::filesystem::path &portDirectory,
                               const bool mayCapture,
                               std::vector<Reached> *reached) {
    std::vector<std::string> args;
    const Profile &profile = config.activeProfile();
    const std::string iwad = beside(config.activeIwadFile(), portDirectory);
    const Dialect::Traits traits = Netgame::launched(profile, Dialect::of(config));

    const Replay replay = replayOf(config, traits);
    const std::filesystem::path demo = replay.leftOut == ReplayLeftOut::none ? replay.file : std::filesystem::path();

    ClassifiedFiles files = classifyFiles(profile.files, portDirectory);
    const DemoUse use = demoUse(profile, demo, !files.lumps.empty());
    std::vector<std::string> always = Text::parse_arguments(config.general.alwaysAdd);
    std::vector<std::string> extra = Text::parse_arguments(profile.extra);

    addSettings(args, reached, config, traits);
    addGame(args, reached, profile, traits, iwad, warpOf(profile.warp, iwad, files.pwads, always, extra));
    addFiles(args, reached, std::move(files), traits, demo);
    const int level = addCompatibility(args, profile, traits, use);
    addDemo(args, reached, profile.replay, traits, demo, replay.source);

    // Ahead of +sv_pistolstart, which a later +dmflags3 would clear.
    addFlags(args, profile, traits);
    addOptions(args, profile, traits, use, level);
    addScreen(args, reached, config, traits);

    const std::filesystem::path save = addSave(args, reached, config, traits, demo);

    reads(reached, Netgame::arguments(args, profile, traits, save));

    if (mayCapture && asksForStdout(config, traits, WINDOWS)) {
        Dialect::add(args, traits, Feature::stdoutSwitch);
    }

    append(args, std::move(always));
    append(args, std::move(extra));

    return args;
}

}

std::string beside(const std::string &file, const std::filesystem::path &directory) {
    if (directory.empty() || file.empty() || std::filesystem::path(file).is_absolute()) {
        return file;
    }

    std::error_code code;
    const std::filesystem::path found = directory / file;

    return std::filesystem::exists(found, code) ? found.string() : file;
}

std::vector<std::string> of(const Config &config, const std::filesystem::path &portDirectory) {
    return build(config, portDirectory, true, nullptr);
}

std::vector<std::string> uncaptured(const Config &config, const std::filesystem::path &portDirectory) {
    return build(config, portDirectory, false, nullptr);
}

Built withReached(const Config &config, const std::filesystem::path &portDirectory) {
    Built built;

    built.arguments = build(config, portDirectory, true, &built.reached);

    return built;
}

Replay replayOf(const Config &config, const Dialect::Traits &traits) {
    Replay out{
            .file = Storage::replayFile(config),
            .source = Storage::replaySource(config),
            .leftOut = ReplayLeftOut::none,
    };
    const ReplayMode mode = config.activeProfile().replay.mode;
    const bool continues = mode == ReplayMode::Continue;

    if (mode == ReplayMode::Off || !traits.has(Feature::replays)) {
        out.leftOut = ReplayLeftOut::off;
    } else if (Netgame::leftOut(config.activeProfile(), traits).has(Feature::replays)) {
        out.leftOut = ReplayLeftOut::server;
    } else if (continues && !traits.has(Feature::continues)) {
        out.leftOut = ReplayLeftOut::cannotContinue;
    } else if (continues && out.source.empty()) {
        out.leftOut = ReplayLeftOut::noSource;
    } else if (out.file.empty()) {
        out.leftOut = ReplayLeftOut::noName;
    }

    return out;
}

bool needsComplevel(const Profile &profile, const Dialect::Traits &traits) {
    const int level = Dialect::pickedLevel(traits.complevel, profile.complevel);

    return profile.replay.mode == ReplayMode::Record && !profile.replay.file.empty()
           && traits.has(Feature::recordsWithComplevel) && (level < 0 || level == traits.unrecordable);
}

bool needsSkill(const Profile &profile, const Dialect::Traits &traits) {
    const ReplayMode mode = profile.replay.mode;
    const bool stops = (mode == ReplayMode::Record && traits.has(Feature::recordsWithSkill))
                       || (mode == ReplayMode::Continue && traits.has(Feature::continuesWithSkill));

    return stops && !profile.replay.file.empty() && profile.skill <= 0;
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

bool asksForStdout(const Config &config, const Dialect::Traits &traits, const bool windows) {
    return windows && Launcher::captures(config, traits) && traits.has(Feature::stdoutSwitch);
}

}
