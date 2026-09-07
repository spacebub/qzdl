/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
 * Copyright (C) 2018-2019  Lcferrum
 * Copyright (C) 2023-2026  spacebub
 *
 * qZDL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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
#include <cstdlib>
#include <fstream>
#include <map>
#include <utility>

#include "core/Launcher.h"
#include "core/MapFile.h"
#include "core/Paths.h"
#include "core/Process.h"
#include "core/Text.h"

namespace {

struct ClassifiedFiles {
    std::vector<std::string> pwads;
    std::vector<std::string> dehs;
    std::vector<std::string> bexs;
    std::vector<std::string> autoexecs;
    std::vector<std::string> lumps;

    // Which of -deh and -bex goes last, decided by whichever kind appeared
    // last in the list. The source port applies the later one on top.
    char dehLast{1};
};

ClassifiedFiles classifyFiles(const std::vector<FileEntry> &files) {
    ClassifiedFiles out;

    for (const FileEntry &entry : files) {
        // Disabled entries stay in the list but off the command line.
        if (!entry.enabled) {
            continue;
        }

        if (Text::iendsWith(entry.file, ".bex")) {
            out.dehLast = 0;
            out.bexs.push_back(entry.file);
        } else if (Text::iendsWith(entry.file, ".deh")) {
            out.dehLast = 1;
            out.dehs.push_back(entry.file);
        } else if (Text::iendsWith(entry.file, ".cfg")) {
            out.autoexecs.push_back(entry.file);
        } else if (Text::iendsWith(entry.file, ".lmp")) {
            out.lumps.push_back(entry.file);
        } else {
            out.pwads.push_back(entry.file);
        }
    }

    return out;
}

std::string iwadPath(const Config &config, const Profile &profile) {
    const NameEntry *iwad = config.findIwad(profile.iwad);

    return iwad != nullptr ? iwad->file : std::string();
}

bool digit(const char letter) {
    return letter >= '0' && letter <= '9';
}

bool letterIs(const char letter, const char wanted) {
    return std::tolower(static_cast<unsigned char>(letter)) == wanted;
}

// A host with the :port taken off the end of it, trailing spaces and all.
std::string withoutPort(const std::string &host) {
    size_t end = host.size();

    while (end > 0 && std::isspace(static_cast<unsigned char>(host[end - 1])) != 0) {
        --end;
    }

    size_t at = end;

    while (at > 0 && digit(host[at - 1])) {
        --at;
    }

    return host.substr(0, at > 0 && host[at - 1] == ':' ? at - 1 : end);
}

// Enough to tell a file written since it was read from one that has not, which is
// as long as the answers below are good for.
struct Stamp {
    std::uintmax_t size{0};
    std::filesystem::file_time_type when;

    friend bool operator==(const Stamp &, const Stamp &) = default;
};

Stamp stampOf(const std::filesystem::path &file) {
    std::error_code code;
    Stamp now;

    now.size = std::filesystem::file_size(file, code);
    now.when = std::filesystem::last_write_time(file, code);

    return now;
}

// What was read out of one file.
struct Known {
    Stamp stamp;
    bool opened{false};
    bool mapxx{false};
    std::vector<std::string> names;
};

// Remembered against the stamp above: reading means walking a whole WAD or PK3
// directory, and it is asked again on every keystroke the preview watches. Both
// answers at once, so a file is opened once for the two.
const Known &readOf(const std::string &file) {
    static std::map<std::string, Known> seen;

    const Stamp now = stampOf(file);

    if (const auto found = seen.find(file); found != seen.end() && found->second.stamp == now) {
        return found->second;
    }

    Known made;

    made.stamp = now;

    if (const std::unique_ptr<MapFile> opened = MapFile::open(file)) {
        made.opened = true;
        made.mapxx = opened->isMapXX();
        made.names = opened->mapNames();
    }

    return seen.insert_or_assign(file, std::move(made)).first->second;
}

/*
Older ports only understand -warp, and what it takes depends on how the IWAD
names its maps: two numbers for ExMy, one for MAPxx. A name that fits neither
is passed as +map, which every modern port understands.
*/
std::vector<std::string> warpArguments(const std::string &iwad, const std::string &map) {
    if (iwad.empty()) {
        return {};
    }

    if (readOf(iwad).mapxx) {
        // MAPxx, two digits and nothing else.
        if (map.size() == 5 && letterIs(map[0], 'm') && letterIs(map[1], 'a')
            && letterIs(map[2], 'p') && digit(map[3]) && digit(map[4])) {
            return {"-warp", map.substr(3, 2)};
        }

        return {};
    }

    // ExMy, one digit for the episode and one from 1 upwards for the map.
    if (map.size() == 4 && letterIs(map[0], 'e') && digit(map[1]) && letterIs(map[2], 'm')
        && map[3] >= '1' && map[3] <= '9') {
        return {"-warp", map.substr(1, 1), map.substr(3, 1)};
    }

    return {};
}

void append(std::vector<std::string> &into, const std::vector<std::string> &what) {
    into.insert(into.end(), what.begin(), what.end());
}

/*
Where the registered IWADs live. -iwad names one file, but an IWAD can require
another beside it -- sve.wad is unplayable without strife1.wad, and Strife
loads voices.wad -- which the port finds only through its own search paths. A
profile with a config of its own starts from a fresh one that knows nowhere.
*/
std::string wadSearchPath(const Config &config) {
#ifdef _WIN32
    constexpr char SEPARATOR = ';';
#else
    constexpr char SEPARATOR = ':';
#endif

    std::vector<std::string> directories;

    for (const NameEntry &iwad : config.iwads) {
        std::error_code code;
        std::string directory = std::filesystem::path(iwad.file).parent_path().string();

        if (directory.empty() || !std::filesystem::is_directory(directory, code)) {
            continue;
        }

        if (std::ranges::find(directories, directory) == directories.end()) {
            directories.push_back(std::move(directory));
        }
    }

    // Whatever the user already set comes first; ZDL only adds to it.
    std::string joined;

    // NOLINTNEXTLINE(concurrency-mt-unsafe) -- only ever reached from the GUI thread.
    if (const char *existing = std::getenv("DOOMWADPATH"); existing != nullptr) {
        joined = existing;
    }

    for (const std::string &directory : directories) {
        if (!joined.empty()) {
            joined.push_back(SEPARATOR);
        }

        joined.append(directory);
    }

    return joined;
}

/*
What a port can be told, since a switch it has never heard of is either passed
over or taken for a file to play. The defaults are the ZDoom family, which is
also what a port nobody here recognises is taken for.
*/
struct Dialect {
    // -iwad. The ports from before Boom look for the game where they were run
    // from or wherever $DOOMWADDIR points instead.
    bool iwad{true};

    // Where the saves go, which the Boom line spells -save. A DOS port keeps
    // them beside itself and has no switch for it at all.
    std::string_view save{"-savedir"};

    // Chocolate Doom keeps its settings in two files: the vanilla half that
    // -config names, and everything on top of it, which is -extraconfig.
    bool extraConfig{false};

    // What the two kinds of DeHackEd patch go on. Only Boom and ZDoom read
    // -bex, Doom Legacy spells it -dehacked, and empty is a port with neither.
    std::string_view deh{"-deh"};
    std::string_view bex{"-bex"};

    // +exec for a .cfg of console commands, and +map for a level that -warp
    // cannot name. Both are ZDoom's own.
    bool exec{true};
    bool map{true};

    bool respawn{true};

    // -host, -join and the rest of a ZDoom netgame. The other families have
    // their own way into one, which is not this.
    bool netplay{true};
};

// The second of the two config files, named after the first.
std::filesystem::path extraConfigFile(const std::filesystem::path &config) {
    return config.parent_path() / (config.stem().string() + "-extra" + config.extension().string());
}

// Which of them a port speaks, told from the name of the program that runs it.
Dialect dialect(const std::filesystem::path &port) {
    static constexpr Dialect ZDOOM{};
    static constexpr Dialect BOOM{
        .save = "-save", .exec = false, .map = false, .netplay = false};
    static constexpr Dialect VANILLA{
        .extraConfig = true, .bex = "-deh", .exec = false, .map = false, .netplay = false};
    static constexpr Dialect HELION{
        .bex = "-deh", .exec = false, .respawn = false, .netplay = false};
    static constexpr Dialect LEGACY{
        .iwad = false, .save = "", .deh = "-dehacked", .bex = "-dehacked", .exec = false,
        .map = false, .netplay = false};
    static constexpr Dialect DOOM{
        .iwad = false, .save = "", .deh = "", .bex = "", .exec = false, .map = false,
        .netplay = false};

    const std::string name = Text::lower(port.stem().string());

    // The DOS releases, which are known by the whole of their name: these are
    // what an id .exe and the ports built straight on top of one were called.
    static constexpr std::array PLAIN = {"doom", "doom2", "doomu", "doom95", "dosdoom"};
    static constexpr std::array LEGACIES = {"doom3", "legacy", "doomlegacy"};

    if (std::ranges::find(PLAIN, name) != PLAIN.end()) {
        return DOOM;
    }

    if (std::ranges::find(LEGACIES, name) != LEGACIES.end()) {
        return LEGACY;
    }

    if (name.contains("helion")) {
        return HELION;
    }

    if (name.contains("chocolate") || name.contains("crispy")) {
        return VANILLA;
    }

    static constexpr std::array BOOMS = {"prboom", "glboom", "dsda", "woof", "nugget",
                                         "eternity", "mbf"};

    for (const char *each : BOOMS) {
        if (name.contains(each)) {
            return BOOM;
        }
    }

    return ZDOOM;
}

/*
DOSBox keeps Z: to itself, and the port's own directory is always C:, so every
other directory a launch names takes a letter from D: up. What runs out is not
the alphabet but DOSBox, which reads ten -c commands and drops the rest without
saying so: eight of them mount, one moves onto C:, and the last is the port.
*/
constexpr char FIRST_DRIVE = 'c';
constexpr char LAST_DRIVE = 'j';

// All of a command line a DOS program is ever handed. Past this the rest of it
// goes into a response file, which the ports read with @.
constexpr size_t DOS_LINE_LIMIT = 126;

/*
Whether DOS can spell the name as it stands. DOSBox shortens anything longer
than 8.3 to something of its own making, which is not what the port was told to
open, so a name that fails here is one the launch is going to lose.
*/
bool spellableInDos(const std::string &name) {
    static constexpr std::string_view EXTRA = "!#$%&'()-@^_`{}~";
    const size_t dot = name.find('.');
    const std::string stem = name.substr(0, dot);

    if (stem.empty() || stem.size() > 8) {
        return false;
    }

    if (dot != std::string::npos) {
        // One dot, and three characters after it.
        const std::string extension = name.substr(dot + 1);

        if (extension.size() > 3 || extension.contains('.')) {
            return false;
        }
    }

    return std::ranges::all_of(name, [](const char letter) {
        const bool plain = (letter >= 'a' && letter <= 'z')
            || (letter >= 'A' && letter <= 'Z')
            || (letter >= '0' && letter <= '9');

        return plain || letter == '.' || EXTRA.contains(letter);
    });
}

/*
Where DOSBox is when the config has not been told. What is on the PATH is the
whole of it on Linux, and the name is tried across the whole of it before the
next one is, so a plain dosbox anywhere beats a variant earlier along. A
Windows installer puts it under Program Files and nothing on the PATH, so those
are looked through as well.
*/
std::filesystem::path findDosbox() {
#ifdef _WIN32
    static constexpr std::array NAMES = {"dosbox.exe", "dosbox-x.exe", "dosbox-staging.exe"};
    constexpr char SEPARATOR = ';';
#else
    static constexpr std::array NAMES = {"dosbox", "dosbox-x", "dosbox-staging"};
    constexpr char SEPARATOR = ':';
#endif

    std::error_code code;

    // NOLINTNEXTLINE(concurrency-mt-unsafe) -- only ever reached from the GUI thread.
    if (const char *path = std::getenv("PATH"); path != nullptr) {
        const std::vector<std::string> directories = Text::split(path, SEPARATOR);

        for (const char *name : NAMES) {
            for (const std::string &directory : directories) {
                if (directory.empty()) {
                    continue;
                }

                if (std::filesystem::path candidate = std::filesystem::path(directory) / name;
                    std::filesystem::is_regular_file(candidate, code)) {
                    return candidate;
                }
            }
        }
    }

#ifdef _WIN32
    // DOSBox-0.74-3, DOSBox-X, dosbox-staging: the version is in the directory
    // name, so what is under Program Files is read rather than guessed at.
    for (const char *variable : {"ProgramFiles", "ProgramFiles(x86)"}) {
        const char *root = std::getenv(variable);

        if (root == nullptr) {
            continue;
        }

        for (const std::filesystem::directory_entry &entry :
             std::filesystem::directory_iterator(root, code)) {
            if (!entry.is_directory(code)
                || !Text::lower(entry.path().filename().string()).starts_with("dosbox")) {
                continue;
            }

            for (const char *name : NAMES) {
                if (std::filesystem::path candidate = entry.path() / name;
                    std::filesystem::is_regular_file(candidate, code)) {
                    return candidate;
                }
            }
        }
    }
#endif

    return {};
}

void say(std::string *error, std::string text) {
    if (error != nullptr) {
        *error = std::move(text);
    }
}

// A program named without a path, found where a shell would look for it.
std::filesystem::path onPath(const std::string &name) {
#ifdef _WIN32
    static constexpr std::array SUFFIXES = {"", ".exe", ".com", ".bat", ".cmd"};
    constexpr char SEPARATOR = ';';
#else
    static constexpr std::array SUFFIXES = {""};
    constexpr char SEPARATOR = ':';
#endif

    // NOLINTNEXTLINE(concurrency-mt-unsafe) -- only ever reached from the GUI thread.
    const char *path = std::getenv("PATH");

    if (path == nullptr) {
        return {};
    }

    std::error_code code;

    for (const std::string &directory : Text::split(path, SEPARATOR)) {
        if (directory.empty()) {
            continue;
        }

        for (const char *suffix : SUFFIXES) {
            if (std::filesystem::path candidate =
                    std::filesystem::path(directory) / (name + suffix);
                std::filesystem::is_regular_file(candidate, code)) {
                return candidate;
            }
        }
    }

    return {};
}

// What one word of a custom command stands for. False is a word that stands
// for nothing, and then the error is what to tell whoever wrote it.
bool substitute(const Config &config, const std::string &name, std::string &value,
                std::string *error) {
    const Profile &profile = config.activeProfile();

    if (name == "source_port") {
        value = Launcher::executable(config).string();

        if (value.empty()) {
            say(error, "This profile has no source port, so there is nothing for {source_port}.");

            return false;
        }

        return true;
    }

    if (name == "game") {
        value = iwadPath(config, profile);

        if (value.empty()) {
            say(error, "This profile has no game, so there is nothing for {game}.");

            return false;
        }

        return true;
    }

    if (name.starts_with("addon_")) {
        const std::string which = name.substr(6);

        if (!Text::isInt(which) || Text::toInt(which) < 1) {
            say(error, "{" + name + "} has to end in a number, counting from one.");

            return false;
        }

        const int wanted = Text::toInt(which);

        if (std::cmp_greater(wanted, profile.files.size())) {
            say(error, profile.files.empty()
                ? "This profile has no add-ons, so there is nothing for {" + name + "}."
                : "This profile has " + std::to_string(profile.files.size())
                  + " add-ons, so there is nothing for {" + name + "}.");

            return false;
        }

        value = profile.files[static_cast<size_t>(wanted - 1)].file;

        return true;
    }

    if (name == "profile" || name == "cfgdir" || name == "savedir") {
        const std::filesystem::path own = Launcher::getConfigPath(config);

        if (own.empty()) {
            say(error, "This profile has no folder of its own, so there is nothing for {"
                + name + "}.");

            return false;
        }

        if (name == "profile") {
            value = own.parent_path().string();
        } else if (name == "cfgdir") {
            value = own.string();
        } else {
            value = Launcher::getSavePath(config).string();
        }

        return true;
    }

    say(error, "{" + name + "} is not one ZDL knows. There is {source_port}, {game}, "
        "{addon_1} upwards, {profile}, {cfgdir} and {savedir}.");

    return false;
}

// The directories a launch reaches into, each one mounted as a drive of its own.
class DosDrives {
public:
    explicit DosDrives(const std::filesystem::path &port, const char last = LAST_DRIVE)
        : _last(last) {
        take(port.parent_path());
    }

    // The DOS spelling of a host file, mounting its directory if it is new.
    std::string spell(const std::filesystem::path &file) {
        const char drive = take(file.parent_path());

        return drive == 0
            ? std::string()
            : std::string(1, drive) + ":\\" + file.filename().string();
    }

    [[nodiscard]] const std::vector<std::pair<char, std::string>> &mounts() const {
        return _mounts;
    }

private:
    char take(const std::filesystem::path &directory) {
        std::error_code code;
        std::string full = std::filesystem::weakly_canonical(directory, code).string();

        if (code || full.empty()) {
            full = directory.string();
        }

        for (const auto &[letter, mounted] : _mounts) {
            if (Text::iequals(mounted, full)) {
                return letter;
            }
        }

        if (_next > _last) {
            return 0;
        }

        _mounts.emplace_back(_next, std::move(full));

        return _next++;
    }

    std::vector<std::pair<char, std::string>> _mounts;
    char _next{FIRST_DRIVE};
    char _last{LAST_DRIVE};
};

// Quoting as DOSBox's own shell reads it, which is only ever about spaces.
std::string dosQuote(const std::string &value) {
    return value.contains(' ') ? "\"" + value + "\"" : value;
}

// A file the launch needs under a name it does not have, copied rather than
// pointed at.
struct Staged {
    std::filesystem::path from;
    std::filesystem::path to;
};

// Everything DOSBox is handed, the copies to put down first, and the response
// file to write if the port's own line came out longer than DOS can take.
struct DosCommand {
    std::vector<std::string> arguments;
    std::vector<Staged> staged;
    std::filesystem::path responseFile;
    std::string responseText;
};

// What id called the games, which is what a port with no -iwad opens by name.
// Nothing else staged beside one is allowed to answer to these.
constexpr std::array IWAD_NAMES = {"doom.wad", "doom1.wad", "doom2.wad", "doomu.wad"};

// Which of them this game is: MAPxx levels are doom2.wad whatever the file is
// called here, and the episodes tell the three Doom releases apart.
std::string dosGameName(const std::string &iwad) {
    const Known &read = readOf(iwad);

    if (!read.opened) {
        return "doom.wad";
    }

    if (read.mapxx) {
        return "doom2.wad";
    }

    bool second = false;

    for (const std::string &map : read.names) {
        if (Text::iequals(map, "E4M1")) {
            return "doomu.wad";
        }

        second = second || Text::iequals(map, "E2M1");
    }

    return second ? "doom.wad" : "doom1.wad";
}

/*
DOSBox renames anything longer than 8.3, and a port from before -iwad opens the
game by a name of its own. Both are answered with a copy under a name that
works, in a directory of ZDL's own that is mounted along with the rest.
*/
class DosStaging {
public:
    explicit DosStaging(std::filesystem::path directory) : _directory(std::move(directory)) {}

    // The file as DOS is going to see it: itself when DOS can spell its name,
    // and a copy under one it can when it cannot.
    std::filesystem::path spellable(const std::filesystem::path &file) {
        return spellableInDos(file.filename().string()) ? file : keep(file, shorten(file));
    }

    // The game under the name the port opens it by, and beside it whatever
    // wads the port keeps for itself.
    std::filesystem::path game(const std::filesystem::path &iwad,
                               const std::filesystem::path &portDirectory) {
        std::error_code code;

        for (std::filesystem::directory_iterator walk(portDirectory, code), end;
             walk != end && !code; walk.increment(code)) {
            const std::string name = Text::lower(walk->path().filename().string());

            if (!name.ends_with(".wad") || !spellableInDos(name)
                || std::ranges::find(IWAD_NAMES, name) != IWAD_NAMES.end()) {
                continue;
            }

            std::error_code asked;

            if (walk->is_regular_file(asked)) {
                keep(walk->path(), name);
            }
        }

        return keep(iwad, dosGameName(iwad.string()));
    }

    [[nodiscard]] const std::filesystem::path &directory() const {
        return _directory;
    }

    [[nodiscard]] const std::vector<Staged> &planned() const {
        return _planned;
    }

private:
    std::filesystem::path keep(const std::filesystem::path &file, const std::string &name) {
        std::filesystem::path to = _directory / name;

        _planned.push_back({.from = file, .to = to});

        return to;
    }

    // A name DOS can spell out of one it cannot: what fits of the stem, the
    // extension cut to three, and a number when that name has been taken.
    std::string shorten(const std::filesystem::path &file) {
        static constexpr std::string_view PLAIN =
            "abcdefghijklmnopqrstuvwxyz0123456789!#$%&'()-@^_`{}~";

        const auto squeeze = [](const std::string &from, const size_t room) {
            std::string kept;

            for (const char letter : from) {
                if (kept.size() == room) {
                    break;
                }

                if (PLAIN.contains(letter)) {
                    kept.push_back(letter);
                }
            }

            return kept;
        };

        // A dot is not one of the characters, so the extension comes back
        // without it either way.
        const std::string tail = squeeze(Text::lower(file.extension().string()), 3);
        const std::string extension = tail.empty() ? std::string() : "." + tail;
        const std::string stem = squeeze(Text::lower(file.stem().string()), 8);
        const std::string base = stem.empty() ? "zdl" : stem;

        std::string wanted = base;

        for (size_t number = 1; std::ranges::any_of(_planned, [&](const Staged &each) {
                 return Text::iequals(each.to.filename().string(), wanted + extension);
             }); number++) {
            const std::string counted = std::to_string(number);

            wanted = base.substr(0, std::min(base.size(), 8 - counted.size())) + counted;
        }

        return wanted + extension;
    }

    std::filesystem::path _directory;
    std::vector<Staged> _planned;
};

// Where the copies go: a hidden directory of ZDL's own, a folder to the
// profile, since a port told to look there writes its settings there too.
std::filesystem::path stagingDirectory(const Config &config, const std::filesystem::path &port) {
    const std::filesystem::path data = Paths::dataDirectory();

    return data.empty()
        ? port.parent_path()
        : data / ".doswaddir" / config.activeProfile().id;
}

/*
The launch as DOSBox takes it: a mount for every directory involved, a move to
the port's own drive, the port itself, and an exit that closes DOSBox with it.
*/
bool buildDosCommand(const Config &config, DosCommand &out, std::string *error) {
    std::error_code code;
    const std::filesystem::path box = Launcher::dosbox(config);

    if (box.empty()) {
        if (error != nullptr) {
            *error = "This machine has no DOSBox on it, and none is set. A DOS source "
                "port needs one to run in, which goes in Settings.";
        }

        return false;
    }

    if (!std::filesystem::exists(box, code)) {
        if (error != nullptr) {
            *error = "DOSBox is not at " + box.string() + " any more.";
        }

        return false;
    }

    const std::filesystem::path port = std::filesystem::absolute(Launcher::executable(config), code);
    const std::string iwad = iwadPath(config, config.activeProfile());

    // The port is opened by name off its own drive, and nothing can rename it
    // there without leaving behind whatever it keeps beside itself.
    if (!spellableInDos(port.filename().string())) {
        if (error != nullptr) {
            *error = "DOSBox renames " + port.filename().string() + " on the way in, and "
                "then there is nothing there by that name to run. Eight characters and "
                "three is all DOS can spell.";
        }

        return false;
    }

    /*
    A port that never heard of -iwad finds the game through $DOOMWADDIR, which
    inside the box is a drive letter and one more command for DOSBox to run.
    It reads ten of those and drops the rest without saying so, so the letter
    it costs comes out of the mounts.
    */
    const bool pointAtGame = !iwad.empty() && !dialect(port).iwad;

    DosDrives drives(port, pointAtGame ? static_cast<char>(LAST_DRIVE - 1) : LAST_DRIVE);
    DosStaging staging(stagingDirectory(config, port));
    std::string wadDrive;

    /*
    The game goes in first, so nothing else staged takes the name it needs.
    Doom Legacy looks for its own doom3.wad wherever it was told the game is
    rather than where it was run from, so the port's wads go in with it.
    */
    if (pointAtGame) {
        const std::string spelled =
            drives.spell(staging.game(std::filesystem::absolute(iwad, code), port.parent_path()));

        if (spelled.empty()) {
            if (error != nullptr) {
                *error = "This launch reaches into more directories than DOSBox will "
                    "mount at once. Keeping the files it loads together would fix it.";
            }

            return false;
        }

        // The drive alone: the port puts the rest of the name on itself.
        wadDrive = spelled.substr(0, 2);
    }

    std::vector<std::string> line;

    // The port is run from its own drive, so it is named without one.
    line.push_back(port.filename().string());

    for (const std::string &argument : Launcher::arguments(config)) {
        // Whatever names a file has to be said in drive letters; the rest of
        // the switches mean the same to a DOS port as to any other.
        if (!std::filesystem::is_regular_file(argument, code)) {
            line.push_back(argument);
            continue;
        }

        std::string spelled =
            drives.spell(staging.spellable(std::filesystem::absolute(argument, code)));

        if (spelled.empty()) {
            if (error != nullptr) {
                *error = "This launch reaches into more directories than DOSBox will "
                    "mount at once. Keeping the files it loads together would fix it.";
            }

            return false;
        }

        line.push_back(std::move(spelled));
    }

    out.staged = staging.planned();

    std::string tail = Text::join({line.begin() + 1, line.end()}, " ");

    /*
    A DOS program is handed 127 characters and no more, which a handful of
    PWADs is already past. The ports have read arguments out of a file since
    the beginning, so a line too long to pass becomes one of those instead.
    */
    if (tail.size() > DOS_LINE_LIMIT) {
        out.responseFile = staging.directory() / "zdl.rsp";
        out.responseText = Text::join({line.begin() + 1, line.end()}, "\n");

        const std::string named = drives.spell(out.responseFile);

        if (named.empty()) {
            if (error != nullptr) {
                *error = "This launch is too long for DOS to take at once, and there "
                    "is no drive left to mount the file that would carry the rest of it.";
            }

            return false;
        }

        tail = "@" + named;
    }

    for (const auto &[letter, directory] : drives.mounts()) {
        out.arguments.emplace_back("-c");
        out.arguments.push_back("mount " + std::string(1, letter) + " " + dosQuote(directory));
    }

    if (!wadDrive.empty()) {
        out.arguments.emplace_back("-c");
        out.arguments.push_back("set DOOMWADDIR=" + wadDrive);
    }

    out.arguments.emplace_back("-c");
    out.arguments.push_back(std::string(1, FIRST_DRIVE) + ":");
    out.arguments.emplace_back("-c");
    out.arguments.push_back(tail.empty() ? line.front() : line.front() + " " + tail);

    // A game is worth the whole screen, and DOSBox opens in a window unless
    // it is told otherwise.
    if (config.activeProfile().dosFullscreen) {
        out.arguments.emplace_back("-fullscreen");
    }

    // Without this DOSBox sits there at a prompt once the game has quit.
    out.arguments.emplace_back("-exit");

    return true;
}

// Where a port looks for what it was not handed outright.
std::map<std::string, std::string> gameEnvironment(const Config &config) {
    std::map<std::string, std::string> environment;
    std::error_code code;

    if (std::string search = wadSearchPath(config); !search.empty()) {
        environment.emplace("DOOMWADPATH", std::move(search));
    }

    /*
    The one the ports actually honour for this. Both are in a stock config's
    search list, but only $DOOMWADDIR resolves a required companion, so it is
    pointed at the directory the profile's own IWAD came out of.
    */
    if (const std::string iwad = iwadPath(config, config.activeProfile()); !iwad.empty()) {
        if (std::filesystem::path const directory = std::filesystem::path(iwad).parent_path();
            !directory.empty() && std::filesystem::is_directory(directory, code)) {
            environment.insert_or_assign("DOOMWADDIR", directory.string());
        }
    }

    return environment;
}

/*
The copies and the response file go down before anything starts, since the port
reads them the moment it does. DOSBox is started out of the port's own
directory, so whatever the port keeps beside itself it still writes there.
*/
bool startInDosbox(const Config &config, Process::Id *id, Process::Stream *output,
                   std::string *error) {
    DosCommand command;

    if (!buildDosCommand(config, command, error)) {
        return false;
    }

    for (const Staged &copy : command.staged) {
        std::error_code code;

        std::filesystem::create_directories(copy.to.parent_path(), code);

        // A game is a dozen megabytes and not worth carrying across for every
        // launch, so the copy stands unless the file has moved on since.
        std::error_code asked;

        if (std::filesystem::exists(copy.to, asked)
            && std::filesystem::file_size(copy.to, asked)
               == std::filesystem::file_size(copy.from, asked)
            && std::filesystem::last_write_time(copy.to, asked)
               >= std::filesystem::last_write_time(copy.from, asked)
            && !asked) {
            continue;
        }

        if (!std::filesystem::copy_file(copy.from, copy.to,
                                        std::filesystem::copy_options::overwrite_existing, code)) {
            if (error != nullptr) {
                *error = "DOS cannot open " + copy.from.filename().string() + " by that name, "
                    "and the copy that would carry it in as "
                    + copy.to.filename().string() + " could not be written: " + code.message();
            }

            return false;
        }
    }

    if (!command.responseFile.empty()) {
        std::error_code code;
        std::filesystem::create_directories(command.responseFile.parent_path(), code);

        if (std::ofstream file(command.responseFile, std::ios::trunc);
            !(file << command.responseText << "\n")) {
            if (error != nullptr) {
                *error = "This launch is longer than DOS can take at once, and "
                    + command.responseFile.string() + ", which would carry the rest of "
                    "it, could not be written.";
            }

            return false;
        }
    }

    std::error_code code;

    return Process::start(Launcher::dosbox(config), command.arguments,
                          std::filesystem::absolute(Launcher::executable(config), code).parent_path(),
                          {}, id, output, error);
}

}

namespace Launcher {

std::filesystem::path executable(const Config &config) {
    const NameEntry *port = config.findPort(config.activeProfile().port);

    return port != nullptr ? std::filesystem::path(port->file) : std::filesystem::path();
}

bool isDosPort(const Config &config) {
    const NameEntry *port = config.findPort(config.activeProfile().port);

    return port != nullptr && port->dosbox;
}

std::filesystem::path systemDosbox() {
    // Looked for once: what is installed does not change under a running ZDL.
    static const std::filesystem::path found = findDosbox();

    return found;
}

std::filesystem::path dosbox(const Config &config) {
    return config.general.dosbox.empty()
        ? systemDosbox()
        : std::filesystem::path(config.general.dosbox);
}

std::filesystem::path getConfigPath(const Profile &profile) {
    if (profile.config.empty()) {
        return {};
    }

    // A hand edited config can name a file anywhere; anything else is a name
    // inside the directory ZDL keeps these in.
    std::filesystem::path named(profile.config);

    if (named.is_absolute()) {
        return named;
    }

    const std::filesystem::path directory = Paths::dataDirectory();

    // A folder to the profile, under the one they all share, with the config at
    // the root of it and whatever the port puts beside it -- its saves -- under
    // that.
    return directory.empty()
        ? std::filesystem::path()
        : directory / "profiles" / named.stem() / named;
}

std::filesystem::path getConfigPath(const Config &config) {
    const Profile &profile = config.activeProfile();

    // A DOS port has never heard of -config, so a profile on one shares.
    if (!config.general.profileConfigs || profile.sharedConfig || isDosPort(config)) {
        return {};
    }

    return getConfigPath(profile);
}

/*
Saves sit under the profile's own folder, so a profile is one place rather
than a config here and a pile of saves in whatever the port shares between
everything. A profile on a shared config shares the port's saves too, which
is what sharing a config means.
*/
std::filesystem::path getSavePath(const Config &config) {
    const std::filesystem::path own = getConfigPath(config);

    return own.empty() ? std::filesystem::path() : own.parent_path() / "saves";
}

std::vector<std::string> arguments(const Config &config) {
    std::vector<std::string> args;
    const Profile &profile = config.activeProfile();
    const std::string iwad = iwadPath(config, profile);
    Dialect speaks = dialect(executable(config));

    // Whatever it is called, nothing out of ZDoom's own console reaches a
    // program written for DOS, and a DOS netgame is a thing of its own.
    if (isDosPort(config)) {
        speaks.map = false;
        speaks.exec = false;
        speaks.netplay = false;
    }

    if (const std::filesystem::path own = getConfigPath(config); !own.empty()) {
        args.emplace_back("-config");
        args.push_back(own.string());

        if (speaks.extraConfig) {
            args.emplace_back("-extraconfig");
            args.push_back(extraConfigFile(own).string());
        }

        if (const std::filesystem::path saves = getSavePath(config);
            !saves.empty() && !speaks.save.empty()) {
            args.emplace_back(speaks.save);
            args.push_back(saves.string());
        }
    }

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

    if (!profile.warp.empty()) {
        if (std::vector<std::string> const warp = warpArguments(iwad, profile.warp); !warp.empty()) {
            append(args, warp);
        } else if (speaks.map) {
            args.emplace_back("+map");
            args.push_back(profile.warp);
        }
    }

    ClassifiedFiles files = classifyFiles(profile.files);

    // A patch a port cannot be handed, and a console script it cannot be told
    // to run, would go on as a file to play instead.
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
        args.emplace_back("-file");
        append(args, files.pwads);
    }

    /*
    Both kinds of patch go on, the one that appeared last in the list last,
    since that is the one applied over the other. The switch is written once
    with every file behind it: a port reads the first -deh and stops looking.
    */
    const bool bexFirst = files.dehLast % 2 != 0;
    const std::vector<std::string> &first = bexFirst ? files.bexs : files.dehs;
    const std::vector<std::string> &second = bexFirst ? files.dehs : files.bexs;

    if (!first.empty()) {
        args.emplace_back(bexFirst ? speaks.bex : speaks.deh);
        append(args, first);
    }

    if (!second.empty()) {
        // The same switch for both kinds is one switch, not two.
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

    const MultiplayerSettings &mp = profile.multiplayer;

    /*
    Only the ZDoom family joins a game this way. A DOS netgame is a different
    thing altogether -- IPX, and a setup program in front of the port -- and
    the Boom and vanilla lines have their own switches for it.
    */
    if (mp.gameType != 0 && speaks.netplay) {
        /*
        A count is what makes this the machine others connect to, and with it
        comes the game they are joining. A joining player is handed all of it
        on connecting, so none of it is written out on that side.
        */
        if (mp.players > 0) {
            if (mp.gameType == 2) {
                args.emplace_back("-deathmatch");
            } else if (mp.gameType == 3) {
                args.emplace_back("-altdeath");
            }

            if (!mp.dmflags.empty()) {
                args.emplace_back("+set");
                args.emplace_back("dmflags");
                args.push_back(mp.dmflags);
            }

            if (!mp.dmflags2.empty()) {
                args.emplace_back("+set");
                args.emplace_back("dmflags2");
                args.push_back(mp.dmflags2);
            }

            args.emplace_back("-host");
            args.push_back(std::to_string(mp.players));

            if (!mp.port.empty()) {
                args.emplace_back("-port");
                args.push_back(mp.port);
            }

            if (!mp.fragLimit.empty()) {
                args.emplace_back("+set");
                args.emplace_back("fraglimit");
                args.push_back(mp.fragLimit);
            }

            if (!mp.timeLimit.empty()) {
                args.emplace_back("+set");
                args.emplace_back("timelimit");
                args.push_back(mp.timeLimit);
            }

            // The host's own save carries the game everyone else drops into.
            if (!mp.savegame.empty()) {
                args.emplace_back("-loadgame");
                args.push_back(mp.savegame);
            }
        } else if (!mp.host.empty()) {
            args.emplace_back("-join");

            if (!mp.port.empty()) {
                // A port typed into the address itself is replaced by the one
                // in the field beside it rather than left on the end.
                args.push_back(withoutPort(mp.host) + ":" + mp.port);
            } else {
                args.push_back(mp.host);
            }
        }

        // How this machine talks, which is its own business either way.
        if (mp.extratic == 1) {
            args.emplace_back("-extratic");
        }

        if (mp.netmode != -1) {
            args.emplace_back("-netmode");
            args.push_back(std::to_string(mp.netmode));
        }

        if (mp.dup != 0) {
            args.emplace_back("-dup");
            args.push_back(std::to_string(mp.dup));
        }
    }

    if (!config.general.alwaysAdd.empty()) {
        append(args, Text::parseArguments(config.general.alwaysAdd));
    }

    if (!profile.extra.empty()) {
        append(args, Text::parseArguments(profile.extra));
    }

    return args;
}

std::vector<std::string> customCommand(const Config &config, std::string *error) {
    const Profile &profile = config.activeProfile();
    const std::vector<std::string> tokens = Text::parseArguments(profile.command);

    if (tokens.empty()) {
        say(error, "There is nothing here to run.");

        return {};
    }

    std::vector<std::string> out;
    out.reserve(tokens.size());

    for (const std::string &token : tokens) {
        std::string filled;
        size_t at = 0;

        while (at < token.size()) {
            const size_t open = token.find('{', at);
            const size_t close = open == std::string::npos
                ? std::string::npos
                : token.find('}', open);

            if (close == std::string::npos) {
                filled.append(token, at);

                break;
            }

            filled.append(token, at, open - at);
            std::string value;

            if (!substitute(config, token.substr(open + 1, close - open - 1), value, error)) {
                return {};
            }

            filled += value;
            at = close + 1;
        }

        out.push_back(std::move(filled));
    }

    return out;
}

std::string commandTemplate(const Config &config) {
    const Profile &profile = config.activeProfile();
    const std::string port = executable(config).string();
    const std::string iwad = iwadPath(config, profile);
    const std::string own = getConfigPath(config).string();
    const std::string saves = getSavePath(config).string();

    const auto spell = [&port, &iwad, &own, &saves, &profile](const std::string &token) {
        if (!port.empty() && token == port) {
            return std::string("{source_port}");
        }

        if (!own.empty() && token == own) {
            return std::string("{cfgdir}");
        }

        if (!saves.empty() && token == saves) {
            return std::string("{savedir}");
        }

        if (!iwad.empty() && token == iwad) {
            return std::string("{game}");
        }

        for (size_t index = 0; index < profile.files.size(); index++) {
            if (profile.files[index].file == token) {
                return "{addon_" + std::to_string(index + 1) + "}";
            }
        }

        return Text::quoteArgument(token);
    };

    std::vector<std::string> parts;

    if (!port.empty()) {
        parts.push_back(spell(port));
    }

    for (const std::string &argument : arguments(config)) {
        parts.push_back(spell(argument));
    }

    return Text::join(parts, " ");
}

std::string commandTrouble(const Config &config) {
    if (!config.activeProfile().customCommand) {
        return {};
    }

    std::string trouble;

    // Only what it would refuse over is wanted, not the command itself.
    [[maybe_unused]] const std::vector<std::string> shown = customCommand(config, &trouble);

    return trouble;
}

std::string commandLine(const Config &config) {
    std::vector<std::string> parts;

    if (config.activeProfile().customCommand) {
        for (const std::string &token : customCommand(config, nullptr)) {
            parts.push_back(Text::quoteArgument(token));
        }

        return Text::join(parts, " ");
    }

    if (isDosPort(config)) {
        DosCommand command;

        // Nothing to say about a launch that cannot be put together; asking to
        // make it is what says so.
        if (!buildDosCommand(config, command, nullptr)) {
            return {};
        }

        parts.push_back(Text::quoteArgument(dosbox(config).string()));

        for (const std::string &argument : command.arguments) {
            parts.push_back(Text::quoteArgument(argument));
        }

        return Text::join(parts, " ");
    }

    const std::filesystem::path port = executable(config);

    if (!port.empty()) {
        parts.push_back(Text::quoteArgument(port.string()));
    }

    for (const std::string &argument : arguments(config)) {
        parts.push_back(Text::quoteArgument(argument));
    }

    return Text::join(parts, " ");
}

bool launch(const Config &config, Process::Id *id, Process::Stream *output, std::string *error) {
    const Profile &profile = config.activeProfile();
    const std::filesystem::path port = executable(config);

    /*
    A profile that writes its own command line is run as it wrote it: whatever
    it names, wherever that lives, and DOSBox only if it asked for one.
    */
    if (profile.customCommand) {
        const std::vector<std::string> tokens = customCommand(config, error);

        if (tokens.empty()) {
            return false;
        }

        std::error_code code;
        std::filesystem::path program(tokens.front());

        // A bare name is one off the PATH, which is where a shell would have
        // found it and where exec will not look.
        if (!program.has_parent_path()) {
            if (std::filesystem::path found = onPath(tokens.front()); !found.empty()) {
                program = std::move(found);
            }
        }

        const std::filesystem::path directory = program.has_parent_path()
            ? std::filesystem::absolute(program, code).parent_path()
            : std::filesystem::path();

        // A directory the command names is one the port writes into, and it
        // writes nothing into a directory that is not there.
        std::error_code made;

        if (profile.command.contains("{profile}") || profile.command.contains("{cfgdir}")) {
            std::filesystem::create_directories(getConfigPath(config).parent_path(), made);
        }

        if (profile.command.contains("{savedir}")) {
            std::filesystem::create_directories(getSavePath(config), made);
        }

        return Process::start(program, {tokens.begin() + 1, tokens.end()}, directory,
                              gameEnvironment(config), id, output, error);
    }

    if (port.empty()) {
        if (error != nullptr) {
            *error = "No source port is selected.";
        }

        return false;
    }

    if (isDosPort(config)) {
        return startInDosbox(config, id, output, error);
    }

    /*
    The port writes its config itself, but only if it has somewhere to write
    it. A directory that cannot be made is not worth failing the launch over.
    The port then says so in its own words, having been started either way.
    */
    if (const std::filesystem::path own = getConfigPath(config); !own.empty()) {
        std::error_code made;
        std::filesystem::create_directories(own.parent_path(), made);
        std::filesystem::create_directories(getSavePath(config), made);
    }

    std::error_code code;
    std::filesystem::path resolved = std::filesystem::absolute(port, code);

    if (code) {
        resolved = port;
    }

    return Process::start(resolved, arguments(config), resolved.parent_path(),
                          gameEnvironment(config), id, output, error);
}

std::vector<std::string> maps(const Config &config) {
    std::vector<std::string> names;
    const Profile &profile = config.activeProfile();

    if (const std::string iwad = iwadPath(config, profile); !iwad.empty()) {
        append(names, readOf(iwad).names);
    }

    for (const FileEntry &entry : profile.files) {
        // Disabled files aren't loaded, so their maps aren't reachable either.
        if (!entry.enabled) {
            continue;
        }

        append(names, readOf(entry.file).names);
    }

    std::ranges::sort(names, Text::naturalLess);
    names.erase(std::ranges::unique(names).begin(), names.end());

    return names;
}

}
