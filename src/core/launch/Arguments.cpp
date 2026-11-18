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
#include "core/wad/HexenMapInfo.h"
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

struct Patch {
    std::string file;
    Feature kind{Feature::deh};
};

struct ClassifiedFiles {
    std::vector<std::string> pwads;
    std::vector<Patch> patches;
    std::vector<std::string> autoexecs;
    std::vector<std::string> lumps;
};

ClassifiedFiles classifyFiles(const std::vector<FileEntry> &files, const std::filesystem::path &directory) {
    ClassifiedFiles out;

    for (const FileEntry &entry : files) {
        // Most ports stop at startup on a file they cannot open.
        if (!entry.enabled || entry.gone) {
            continue;
        }

        if (Text::iends_with(entry.file, ".bex")) {
            out.patches.push_back({.file = beside(entry.file, directory), .kind = Feature::bex});
        } else if (Text::iends_with(entry.file, ".deh") || Text::iends_with(entry.file, ".hhe")
                   || Text::iends_with(entry.file, ".seh")) {
            out.patches.push_back({.file = beside(entry.file, directory), .kind = Feature::deh});
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

    // For a port whose -warp takes the warptrans number of the map.
    int warpTrans{0};
};

bool warpsByWarpTrans(const Dialect::Traits &traits) {
    return traits.warpNumber == Dialect::WarpNumber::warpTrans;
}

std::vector<std::string> warpArguments(const Warp &warp, const std::string &map, const Dialect::Traits &traits) {
    if (!warp.numbers) {
        return {};
    }

    if (warpsByWarpTrans(traits)) {
        return {std::to_string(warp.warpTrans)};
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

void append(std::vector<std::string> &into, const std::vector<std::string> &what) {
    into.insert(into.end(), what.begin(), what.end());
}

void append(std::vector<std::string> &into, std::vector<std::string> &&what) {
    into.insert(into.end(), std::make_move_iterator(what.begin()), std::make_move_iterator(what.end()));
}

// The words that start the port on the map, none when the line sends no map.
std::vector<std::string> mapArguments(const Warp &warp, const std::string &map, const Dialect::Traits &traits) {
    std::vector<std::string> words;

    if (!warp.offered) {
        return words;
    }

    if (std::vector<std::string> numbers = warpArguments(warp, map, traits);
        !numbers.empty() && Dialect::add(words, traits, Feature::warp)) {
        append(words, std::move(numbers));
    } else {
        Dialect::add(words, traits, Feature::map, map);
    }

    return words;
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

enum class DemoUse : std::uint8_t {
    none,
    records,
    plays,
    continues,
};

// Below it the PrBoom line plays and records as vanilla Doom.
constexpr int FIRST_BOOM_LEVEL = 7;

constexpr int NO_MONSTERS = 1;
constexpr int FAST = 2;
constexpr int RESPAWN = 3;
constexpr int FAST_AND_RESPAWN = 4;

constexpr std::array MONSTERS = {NO_MONSTERS, FAST, RESPAWN, FAST_AND_RESPAWN};

constexpr std::array MONSTER_SWITCHES = {Feature::noMonsters, Feature::fast, Feature::respawn};

// Which of MONSTER_SWITCHES the port reads, one bit each.
using MonsterReads = std::size_t;

constexpr MonsterReads READS_NO_MONSTERS = 1;
constexpr MonsterReads READS_FAST = 2;
constexpr MonsterReads READS_RESPAWN = 4;
constexpr MonsterReads READS_ALL = READS_NO_MONSTERS | READS_FAST | READS_RESPAWN;

MonsterReads monsterReads(const Dialect::Traits &traits) {
    return (traits.has(Feature::noMonsters) ? READS_NO_MONSTERS : 0) | (traits.has(Feature::fast) ? READS_FAST : 0)
           | (traits.has(Feature::respawn) ? READS_RESPAWN : 0);
}

constexpr int playedWith(const int monsters, const MonsterReads reads) {
    if (monsters < NO_MONSTERS) {
        return monsters;
    }

    if (monsters == NO_MONSTERS) {
        return (reads & READS_NO_MONSTERS) != 0 ? NO_MONSTERS : 0;
    }

    const bool fast = monsters % 2 == 0 && (reads & READS_FAST) != 0;
    const bool respawn = monsters >= RESPAWN && (reads & READS_RESPAWN) != 0;

    if (respawn) {
        return fast ? FAST_AND_RESPAWN : RESPAWN;
    }

    return fast ? FAST : 0;
}

struct MonsterChoices {
    std::array<int, MONSTERS.size()> values{};
    std::size_t size = 0;
};

// A value is offered where it plays as itself.
constexpr std::array MONSTER_CHOICES = [] {
    std::array<MonsterChoices, READS_ALL + 1> out{};

    for (MonsterReads reads = 0; reads < out.size(); ++reads) {
        for (const int value : MONSTERS) {
            if (playedWith(value, reads) == value) {
                out[reads].values[out[reads].size] = value;
                ++out[reads].size;
            }
        }
    }

    return out;
}();

// The Arg::refused masks that hold for this launch.
std::uint32_t occasion(const Profile &profile, const Dialect::Traits &traits, const DemoUse use) {
    std::uint32_t now = Netgame::inPlay(profile, traits) ? Dialect::NETGAME : 0U;

    if (use == DemoUse::plays) {
        now |= Dialect::PLAYBACK;
    } else if (use != DemoUse::none) {
        now |= Dialect::RECORDING;
    }

    return now;
}

Dialect::Traits withoutRefused(Dialect::Traits traits, const std::uint32_t now) {
    if (now == 0) {
        return traits;
    }

    for (std::size_t at = 0; at < static_cast<std::size_t>(Feature::count); ++at) {
        const auto feature = static_cast<Feature>(at);

        if (traits.has(feature) && (Dialect::arg(traits, feature).refused & now) != 0) {
            traits.features &= ~Dialect::Features(feature);
        }
    }

    return traits;
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

    if (const int monsters = playedMonsters(profile.monsters, traits); monsters == NO_MONSTERS) {
        Dialect::add(args, traits, Feature::noMonsters);
    } else if (monsters > NO_MONSTERS) {
        if (monsters % 2 == 0) {
            Dialect::add(args, traits, Feature::fast);
        }

        if (monsters >= RESPAWN) {
            Dialect::add(args, traits, Feature::respawn);
        }
    }

    if (profile.skill > 0) {
        Dialect::add(args, traits, Feature::skill, std::to_string(profile.skill));
    }

    append(args, mapArguments(warp, profile.warp, traits));
}

void addPatches(std::vector<std::string> &args,
                std::vector<Reached> *reached,
                const ClassifiedFiles &files,
                const Dialect::Traits &traits) {
    if (files.patches.empty()) {
        return;
    }

    // Every port spells deh and bex alike, so one switch takes every patch in list order.
    Dialect::add(args, traits, files.patches.front().kind);

    for (const Patch &patch : files.patches) {
        reads(reached, patch.file);
        args.push_back(patch.file);
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

    std::erase_if(files.patches, [&traits](const Patch &patch) { return !traits.has(patch.kind); });

    // The port loads the first patch in the list and drops the rest without a word.
    if (traits.has(Feature::onePatch) && files.patches.size() > 1) {
        files.patches.resize(1);
    }

    if (!files.pwads.empty() && Dialect::add(args, traits, Feature::pwads)) {
        reads(reached, files.pwads);
        append(args, std::move(files.pwads));
    }

    addPatches(args, reached, files, traits);

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
    return (traits.has(Feature::recordsBareName) ? demo.parent_path() / demo.stem() : demo).string();
}

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
                const int level) {
    const bool joins = Netgame::joins(profile, traits);

    for (const Options::Option &option : Options::ALL) {
        const bool fromHost = joins && option.group == Options::Group::rules;

        if (fromHost || !traits.has(option.feature) || profile.stored(option).empty()) {
            continue;
        }

        const Dialect::Arg &arg = Dialect::arg(traits, option.feature);

        if (!Dialect::worksAt(arg, level)) {
            continue;
        }

        if (option.kind == Options::Kind::toggle) {
            Dialect::add(args, traits, option.feature);
        } else if (const std::optional<int> value = Dialect::parse(arg, profile.stored(option))) {
            Dialect::add(args, traits, option.feature, std::to_string(*value));
        }
    }
}

// Most ports keep no trace of it in the demo, which then desyncs when played back without it.
void addSoloNet(std::vector<std::string> &args, const ReplaySettings &replay, const Dialect::Traits &traits) {
    if (replay.soloNet) {
        Dialect::add(args, traits, Feature::soloNet);
    }
}

void addContinue(std::vector<std::string> &args,
                 std::vector<Reached> *reached,
                 const ReplaySettings &replay,
                 const Dialect::Traits &traits,
                 const std::filesystem::path &demo,
                 const std::filesystem::path &from) {
    // Playback reads the old demo through the same setting, so it has to match.
    if (longticsDemo(from)) {
        Dialect::add(args, traits, Feature::longtics);
    }

    addSoloNet(args, replay, traits);
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
             const std::filesystem::path &source,
             const int level) {
    if (!demo.empty()) {
        if (replay.mode == ReplayMode::Continue) {
            addContinue(args, reached, replay, traits, demo, source);
        } else if (replay.mode == ReplayMode::Record) {
            // These must precede -record. They decide the demo header.
            if (replay.longtics && Dialect::worksAt(Dialect::arg(traits, Feature::longtics), level)) {
                Dialect::add(args, traits, Feature::longtics);
            }

            addSoloNet(args, replay, traits);

            if (Dialect::add(args, traits, Feature::record, recordedAs(demo, traits))) {
                writesBeside(reached, demo);
            }
        } else {
            // A client demo under -timedemo plays untimed and never quits.
            const Playback playback = Storage::isClientDemo(demo, traits) ? Playback::AsRecorded : replay.playback;
            Dialect::Feature how = Feature::playdemo;

            if (playback == Playback::Timed && traits.has(Feature::timedemo)) {
                how = Feature::timedemo;
            } else if (playback == Playback::Fast && traits.has(Feature::fastdemo)) {
                how = Feature::fastdemo;
            }

            addSoloNet(args, replay, traits);

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
    if (!save.empty() && !Storage::saveThere(traits, save)) {
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

// What the line is built from, read once for build() and for the answers that must agree with it.
struct Launch {
    Dialect::Traits traits;
    std::string iwad;
    ClassifiedFiles files;
    Replay replay;
    std::filesystem::path demo;
    DemoUse use{DemoUse::none};
    std::vector<std::string> always;
    std::vector<std::string> extra;

    // Where the port looks for a relative file typed on the line, in order: where it runs, its own folder, then the
    // IWAD folder the launch hands it as DOOMWADDIR.
    std::vector<std::filesystem::path> typedFolders;
};

Launch launchOf(const Config &config, const std::filesystem::path &portDirectory) {
    const Profile &profile = config.activeProfile();
    const Dialect::Traits launched = Netgame::launched(profile, Dialect::of(config));

    Launch out{
            .traits = launched,
            .iwad = beside(config.activeIwadFile(), portDirectory),
            .files = classifyFiles(profile.files, portDirectory),
            .replay = replayOf(config, launched),
            .demo = {},
            .use = DemoUse::none,
            .always = Text::parse_arguments(config.general.alwaysAdd),
            .extra = Text::parse_arguments(profile.extra),
            .typedFolders = {},
    };

    out.typedFolders = {
            Storage::plannedRunDirectory(config, portDirectory),
            portDirectory,
            std::filesystem::path(out.iwad).parent_path(),
    };

    if (out.replay.leftOut == ReplayLeftOut::none) {
        out.demo = out.replay.file;
    }

    out.use = demoUse(profile, out.demo, !out.files.lumps.empty());

    // The traits lose every switch the port would stop on, so no part of the line sends one.
    out.traits = withoutRefused(launched, occasion(profile, launched, out.use));

    return out;
}

// The file in the first folder that holds it, or the name as typed.
std::string foundIn(const std::string &file, const std::span<const std::filesystem::path> folders) {
    for (const std::filesystem::path &folder : folders) {
        if (std::string found = beside(file, folder); found != file) {
            return found;
        }
    }

    return file;
}

// The first -merge group and the first -file group, as M_CheckParmWithArgs finds them. A group runs to the next word
// that starts with a dash.
struct FileGroups {
    std::vector<std::string> merged;
    std::vector<std::string> filed;
    bool mergeSeen{false};
    bool fileSeen{false};

    void read(const std::vector<std::string> &words, const std::span<const std::filesystem::path> folders) {
        for (size_t at = 0; at < words.size(); ++at) {
            const bool merge = Text::iequals(words[at], "-merge");

            if (!merge && !Text::iequals(words[at], "-file")) {
                continue;
            }

            bool &seen = merge ? mergeSeen : fileSeen;

            // A switch that ends the line is not found.
            if (seen || at + 1 == words.size()) {
                continue;
            }

            seen = true;

            while (at + 1 < words.size() && !words[at + 1].starts_with('-')) {
                (merge ? merged : filed).push_back(foundIn(words[++at], folders));
            }
        }
    }
};

// The files Hexen loads in order, as W_ParseCommandLine reads the line: the IWAD, the merged files, then the -file
// group. Autoload folders are not read. A bare typed word that would run on from the group of the add-ons is not
// counted.
std::vector<std::string> hexenLoads(const Launch &launch) {
    std::vector<std::string> own;
    std::vector<std::string> typed = launch.always;
    FileGroups groups;

    if (!launch.files.pwads.empty() && Dialect::add(own, launch.traits, Feature::pwads)) {
        append(own, launch.files.pwads);
    }

    append(typed, launch.extra);
    groups.read(own, launch.typedFolders);
    groups.read(typed, launch.typedFolders);

    std::vector<std::string> files;

    if (!launch.iwad.empty()) {
        files.push_back(launch.iwad);
    }

    append(files, std::move(groups.merged));
    append(files, std::move(groups.filed));

    return files;
}

// A map whose add-on is now off or gone stops most ports at startup. The choice is
// trusted unless every file that could hold maps was read and none of them does.
// A port that warps by warptrans goes only to a map the files list.
Warp warpOf(const std::string &map, const Launch &launch) {
    if (map.empty()) {
        return {};
    }

    if (warpsByWarpTrans(launch.traits)) {
        const auto number = static_cast<size_t>(HexenMapInfo::mapNumber(map));

        if (number == 0) {
            return {};
        }

        const HexenMapInfo::Warps warps = HexenMapInfo::warps(hexenLoads(launch));

        if (!warps.reached.test(number)) {
            return {};
        }

        return Warp{.offered = true, .numbers = true, .mapxx = false, .warpTrans = warps.warpTrans.at(number)};
    }

    if (launch.iwad.empty()) {
        return Warp{.offered = true, .numbers = false, .mapxx = false, .warpTrans = 0};
    }

    // An unread file may hold the map, which settles the answer without reading the rest.
    const auto lists = [&map](const MapFile::Maps &maps) {
        return !maps.opened
               || std::ranges::any_of(maps.names, [&map](const std::string &name) { return Text::iequals(name, map); });
    };

    // Taken before the add-ons are read, which may drop the iwad from the cache.
    const MapFile::Maps &game = MapFile::maps(launch.iwad);
    const bool mapxx = game.mapxx;
    const bool listed = lists(game);

    const auto pwadLists = [&lists](const std::string &file) { return lists(MapFile::maps(file)); };
    const bool offered = listed || std::ranges::any_of(launch.files.pwads, pwadLists) || typesFiles(launch.always)
                         || typesFiles(launch.extra);

    return Warp{.offered = offered, .numbers = true, .mapxx = mapxx, .warpTrans = 0};
}

std::vector<std::string> build(const Config &config,
                               const std::filesystem::path &portDirectory,
                               const bool mayCapture,
                               std::vector<Reached> *reached) {
    std::vector<std::string> args;
    const Profile &profile = config.activeProfile();
    Launch launch = launchOf(config, portDirectory);
    const Dialect::Traits &traits = launch.traits;
    const std::filesystem::path &demo = launch.demo;

    addSettings(args, reached, config, traits);
    addGame(args, reached, profile, traits, launch.iwad, warpOf(profile.warp, launch));
    addFiles(args, reached, std::move(launch.files), traits, demo);
    const int level = addCompatibility(args, profile, traits, launch.use);
    addDemo(args, reached, profile.replay, traits, demo, launch.replay.source, level);

    // Ahead of +sv_pistolstart, which a later +dmflags3 would clear.
    addFlags(args, profile, traits);
    addOptions(args, profile, traits, level);
    addScreen(args, reached, config, traits);

    const std::filesystem::path save = addSave(args, reached, config, traits, demo);

    reads(reached, Netgame::arguments(args, profile, traits, save));

    if (mayCapture && asksForStdout(config, traits, WINDOWS)) {
        Dialect::add(args, traits, Feature::stdoutSwitch);
    }

    append(args, std::move(launch.always));
    append(args, std::move(launch.extra));

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

std::span<const int> monsterChoices(const Dialect::Traits &traits) {
    const MonsterChoices &choices = MONSTER_CHOICES[monsterReads(traits)];

    return {choices.values.data(), choices.size};
}

int playedMonsters(const int monsters, const Dialect::Traits &traits) {
    return playedWith(monsters, monsterReads(traits));
}

std::string monstersHint(const Dialect::Traits &traits) {
    std::string out;

    for (const Feature feature : MONSTER_SWITCHES) {
        if (!traits.has(feature) || Dialect::arg(traits, feature).refused == 0) {
            continue;
        }

        if (!out.empty()) {
            out += ' ';
        }

        out += Dialect::describe(traits, feature);
    }

    return out;
}

bool takesMap(const Dialect::Traits &traits) {
    return traits.has(Feature::warp) || traits.has(Feature::map);
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

TakenDemo takenDemo(const Profile &profile, const Dialect::Traits &traits) {
    if (traits.has(Feature::numbersTakenDemo)) {
        return TakenDemo::numbers;
    }

    // A continued demo is written afresh, since it goes on from the demo it plays.
    if (profile.replay.mode == ReplayMode::Continue) {
        return TakenDemo::writesOver;
    }

    const int level = Dialect::pickedLevel(traits.complevel, profile.complevel);
    const bool vanilla = level >= 0 && level < FIRST_BOOM_LEVEL;

    if (traits.has(Feature::continuesTakenDemo) && !(vanilla && traits.has(Feature::restartsVanillaDemo))) {
        return traits.has(Feature::stopsWithoutSave) ? TakenDemo::continuesOrStops : TakenDemo::continuesOrWritesOver;
    }

    return vanilla && traits.has(Feature::stopsOnVanillaDemo) ? TakenDemo::stops : TakenDemo::writesOver;
}

std::vector<std::string> maps(const Config &config) {
    std::vector<std::string> names;
    const Profile &profile = config.activeProfile();

    if (warpsByWarpTrans(Dialect::of(config))) {
        const HexenMapInfo::Warps warps =
                HexenMapInfo::warps(hexenLoads(launchOf(config, Launcher::portPath(config).directory)));

        for (int map = 1; map <= HexenMapInfo::LAST_WARPED; ++map) {
            if (warps.reached.test(static_cast<size_t>(map))) {
                names.push_back(HexenMapInfo::lumpOf(map));
            }
        }

        return names;
    }

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

std::string mapsKey(const Config &config) {
    const Profile &profile = config.activeProfile();
    std::string key = config.activeIwadFile();

    for (const FileEntry &entry : profile.files) {
        if (entry.enabled) {
            key += '\0';
            key += entry.file;
            key += entry.gone ? '1' : '0';
        }
    }

    if (const Dialect::Traits traits = Dialect::of(config); warpsByWarpTrans(traits)) {
        const std::filesystem::path portDirectory = Launcher::portPath(config).directory;

        // Hexen also loads the files typed on the line, and finds them where it runs.
        for (const std::string &part : {
                     std::string(Dialect::arg(traits, Feature::pwads).name),
                     config.general.alwaysAdd,
                     profile.extra,
                     portDirectory.string(),
                     Storage::plannedRunDirectory(config, portDirectory).string(),
             }) {
            key += '\0';
            key += part;
        }
    }

    return key;
}

std::string startedMap(const Config &config) {
    const std::string &map = config.activeProfile().warp;

    if (map.empty()) {
        return {};
    }

    const Launch launch = launchOf(config, Launcher::portPath(config).directory);

    return mapArguments(warpOf(map, launch), map, launch.traits).empty() ? std::string() : map;
}

bool asksForStdout(const Config &config, const bool windows) {
    return windows && asksForStdout(config, Dialect::of(config), true);
}

bool asksForStdout(const Config &config, const Dialect::Traits &traits, const bool windows) {
    return windows && Launcher::captures(config, traits) && traits.has(Feature::stdoutSwitch);
}

}
