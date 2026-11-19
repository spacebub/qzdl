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
#include <fstream>
#include <utility>

#include "ttk/system/Text.h"

#include "core/config/Schema.h"
#include "core/launch/Arguments.h"
#include "core/launch/Dialect.h"
#include "core/launch/Dos.h"
#include "core/launch/DosFiles.h"
#include "core/launch/FlatpakRun.h"
#include "core/launch/KnownPorts.h"
#include "core/launch/Launcher.h"
#include "core/launch/Storage.h"

using namespace ttk;

namespace Dos {

namespace {

// C: is the port's own directory. Mounts share the eleven -c commands DOSBox honours with the drive change and the
// run, plus the exit, DOOMWADDIR, or PATH and ALLEGRO for a port in its run folder.
constexpr int FIXED_COMMANDS = 2;
constexpr int RUN_FOLDER_COMMANDS = 2;
constexpr char FIRST_DRIVE = 'c';
constexpr char LAST_DRIVE = 'j';

// DOSBox keeps the space before the arguments within its 126 bytes. Past this they go into a response file.
constexpr size_t DOS_LINE_LIMIT = 125;

constexpr const char *TOO_MANY_DIRECTORIES =
        "This launch reaches into more directories than DOSBox will mount at once. The "
        "port's own folder takes one of them, so keeping the files it loads together "
        "would free the rest.";

class DosDrives {
public:
    // The first mount takes C:, which is where DOSBox lands.
    DosDrives(const std::filesystem::path &landing, const int mounts)
        : _last(static_cast<char>(std::min(FIRST_DRIVE + mounts - 1, +LAST_DRIVE))) {
        driveFor(landing);
    }

    // Reached through its parent's drive rather than taking one of its own.
    void within(const std::filesystem::path &directory, const std::filesystem::path &parent, const std::string &name) {
        _within.push_back({.directory = directory, .parent = parent, .name = name});
    }

    std::string spellDirectory(const std::filesystem::path &directory) {
        for (const Within &each : _within) {
            if (each.directory == directory) {
                const std::string parent = spellDirectory(each.parent);

                return parent.empty() ? parent : parent + "\\" + each.name;
            }
        }

        const char drive = driveFor(directory);

        return drive == 0 ? std::string() : std::string(1, drive) + ":";
    }

    std::string spell(const std::filesystem::path &file) {
        const std::string where = spellDirectory(file.parent_path());

        return where.empty() ? std::string() : where + "\\" + file.filename().string();
    }

    [[nodiscard]] const std::vector<std::pair<char, std::string>> &mounts() const { return _mounts; }

private:
    struct Within {
        std::filesystem::path directory;
        std::filesystem::path parent;
        std::string name;
    };

    char driveFor(const std::filesystem::path &directory) {
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

    std::vector<Within> _within;
    std::vector<std::pair<char, std::string>> _mounts;
    char _next{FIRST_DRIVE};
    char _last{LAST_DRIVE};
};

std::string dosQuote(const std::string &value) {
    const bool quoted = value.size() >= 2 && value.front() == '"' && value.back() == '"';

    return value.contains(' ') && !quoted ? "\"" + value + "\"" : value;
}

struct Built {
    std::filesystem::path program;
    std::vector<std::string> arguments;
    std::filesystem::path portDirectory;
    DosFiles::Directories where;
    std::vector<DosFiles::Copy> staged;
    std::filesystem::path responseFile;
    std::string responseText;
    Arguments::Notes notes;
};

bool build(const Config &config, Built &out, std::string *error) {
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

    const Profile &profile = config.activeProfile();
    const std::filesystem::path file = std::filesystem::absolute(Launcher::executable(config), code);
    const std::string iwad = config.activeIwadFile();

    if (!DosFiles::spellable(file.filename().string())) {
        if (error != nullptr) {
            *error = "DOSBox renames " + file.filename().string()
                     + " on the way in, and "
                       "then there is nothing there by that name to run. Eight characters and "
                       "three is all DOS can spell.";
        }

        return false;
    }

    const Dialect::Traits traits = Dialect::of(config);

    out.portDirectory = file.parent_path();
    out.where = DosFiles::directories(config, file);

    const bool here = out.where.runsHere;
    const Port::DosRun run = Port::known(traits.port).dos.value_or(Port::DosRun{});

    // A pre-Boom port takes no -iwad. In its run folder it gets a copy of the game, else it searches $DOOMWADDIR or
    // its own directory.
    DosFiles::Reach game = DosFiles::Reach::beside;

    if (!traits.has(Dialect::Feature::iwad) && !iwad.empty()) {
        game = here ? DosFiles::Reach::staged
                    : DosFiles::reach(std::filesystem::absolute(iwad, code),
                                      out.portDirectory,
                                      traits.has(Dialect::Feature::recognised));
    }

    const bool points = game != DosFiles::Reach::beside && !here;
    const int fixed = FIXED_COMMANDS + (here ? RUN_FOLDER_COMMANDS : 0) + (points ? 1 : 0)
                      + (profile.on(Switch::dosExit) ? 1 : 0);

    DosDrives drives(out.portDirectory, COMMANDS - fixed);

    // The run folder takes D:, where DOSBox lands.
    const std::string landing = here ? drives.spellDirectory(out.where.instance) : std::string(1, FIRST_DRIVE) + ":";

    drives.within(out.where.files, out.where.instance, ConfigFile::DOS_FILES_DIR);

    if (const std::filesystem::path replays = Storage::replayDirectory(config); !replays.empty()) {
        drives.within(replays, replays.parent_path(), replays.filename().string());
    }

    DosFiles::Staging staging(out.where);

    std::string wadDirectory;

    // The recording does not exist yet, so the loop below cannot stat it.
    const std::filesystem::path recording =
            profile.replay.mode == ReplayMode::Record ? Storage::replayFile(config) : std::filesystem::path();
    const std::string recorded =
            recording.empty() ? std::string() : (recording.parent_path() / recording.stem()).string();

    // Staged first so nothing else takes its name.
    if (game == DosFiles::Reach::staged) {
        staging.game(std::filesystem::absolute(iwad, code), run.doomu);
        wadDirectory = here ? std::string() : drives.spellDirectory(out.where.instance);
    } else if (game == DosFiles::Reach::pointed) {
        wadDirectory = drives.spellDirectory(std::filesystem::absolute(iwad, code).parent_path());
    }

    if (points && wadDirectory.empty()) {
        if (error != nullptr) {
            *error = TOO_MANY_DIRECTORIES;
        }

        return false;
    }

    if (here) {
        staging.beside(file);

        for (const std::string_view name : run.companions) {
            if (name.empty()) {
                continue;
            }

            if (const std::filesystem::path data = DosFiles::inFolder(out.portDirectory, std::string(name));
                std::filesystem::is_regular_file(data, code)) {
                staging.beside(data);
            }
        }
    }

    std::vector<std::string> line;

    // A batch file run bare takes the shell with it, and nothing after it runs.
    line.push_back(DosFiles::isBatch(file) ? "call " + file.filename().string() : file.filename().string());

    Arguments::Built generated = Arguments::withNotes(config, out.portDirectory);

    out.notes = std::move(generated.notes);

    for (const std::string &argument : generated.arguments) {
        const bool records = !recorded.empty() && argument == recorded;

        // One directory_entry answers both questions off one stat. This loop runs
        // over every argument, and the preview builds the line twice.
        const std::filesystem::directory_entry entry(argument, code);

        if (!records && entry.is_directory(code)) {
            if (error != nullptr) {
                *error = "A DOS port cannot load a folder. " + argument
                         + " would have to be "
                           "a WAD or a PK3 for this profile to launch.";
            }

            return false;
        }

        if (!records && !entry.is_regular_file(code)) {
            line.push_back(argument);
            continue;
        }

        // Vanilla plays a demo only by the bare name of a file where it runs.
        if (here && run.bareDemo
            && (line.back() == Dialect::arg(traits, Dialect::Feature::playdemo).name
                || line.back() == Dialect::arg(traits, Dialect::Feature::timedemo).name)) {
            line.push_back(staging.demo(std::filesystem::absolute(argument, code)));
            continue;
        }

        if (records && !DosFiles::spellable(recording.filename().string())) {
            if (error != nullptr) {
                *error = "DOS cannot spell " + recording.filename().string()
                         + ", so there "
                           "would be nothing by that name to record into. Eight characters and "
                           "three is all it can spell.";
            }

            return false;
        }

        std::string spelled = records ? drives.spell(recording.parent_path() / recording.stem())
                                      : drives.spell(staging.spellableName(std::filesystem::absolute(argument, code)));

        if (spelled.empty()) {
            if (error != nullptr) {
                *error = TOO_MANY_DIRECTORIES;
            }

            return false;
        }

        line.push_back(std::move(spelled));
    }

    out.staged = staging.planned();

    std::vector<std::string> grouped;

    grouped.reserve(line.size());

    for (auto each = line.begin() + 1; each != line.end(); ++each) {
        grouped.push_back(dosQuote(*each));
    }

    std::string tail = Text::join(grouped, " ");

    if (tail.size() > DOS_LINE_LIMIT) {
        out.responseFile = out.where.instance / "zdl.rsp";

        // A response file is split on whitespace alone, so quotes there would reach the port as text.
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

    if (const std::filesystem::path tuning = out.portDirectory / "dosbox.conf";
        std::filesystem::is_regular_file(tuning, code)) {
        out.arguments.emplace_back("-conf");
        out.arguments.push_back(tuning.string());
    }

    for (const auto &[letter, directory] : drives.mounts()) {
        out.arguments.emplace_back("-c");
        out.arguments.push_back("mount " + std::string(1, letter) + " " + dosQuote(directory));
    }

    if (!wadDirectory.empty()) {
        out.arguments.emplace_back("-c");
        out.arguments.push_back("set DOOMWADDIR=" + wadDirectory);
    }

    // The copy runs away from the port folder. DOS/4GW and CWSDPMI are found on PATH, and Allegro reads its sound
    // setup from the folder ALLEGRO names, else from beside the program.
    if (here) {
        out.arguments.emplace_back("-c");
        out.arguments.emplace_back("set PATH=%PATH%;C:\\");
        out.arguments.emplace_back("-c");
        out.arguments.emplace_back("set ALLEGRO=C:\\");
    }

    out.arguments.emplace_back("-c");
    out.arguments.push_back(landing);
    out.arguments.emplace_back("-c");
    out.arguments.push_back(tail.empty() ? line.front() : line.front() + " " + tail);

    // -exit closes DOSBox only for a program named as a bare argument, never for one
    // run with -c, so the shell is told to quit instead.
    if (profile.on(Switch::dosExit)) {
        out.arguments.emplace_back("-c");
        out.arguments.emplace_back("exit");
    }

    if (profile.on(Switch::dosFullscreen)) {
        out.arguments.emplace_back("-fullscreen");
    }

    out.program = box;

    // A Flatpak DOSBox sees only the folders it is granted, so each mounted one is.
    if (Flatpak::exported(box)) {
        std::vector<Arguments::Reached> reached;

        for (const auto &[letter, directory] : drives.mounts()) {
            reached.push_back({.path = directory, .access = Arguments::Access::writes});
        }

        FlatpakRun::Command sandboxed = FlatpakRun::command(
                {
                        .file = box,
                        .reached = std::move(reached),
                        .runDirectory = {},
                        .gameFolder = {},
                        .variables = Launcher::variables(profile),
                },
                error);

        if (sandboxed.arguments.empty()) {
            return false;
        }

        sandboxed.arguments.insert(sandboxed.arguments.end(),
                                   std::make_move_iterator(out.arguments.begin()),
                                   std::make_move_iterator(out.arguments.end()));
        out.program = std::move(sandboxed.flatpak);
        out.arguments = std::move(sandboxed.arguments);
    }

    return true;
}

}

bool start(const Config &config, Process::Id *id, Process::Stream *output, std::string *error) {
    Built command;

    if (!build(config, command, error)) {
        return false;
    }

    for (const DosFiles::Copy &copy : command.staged) {
        std::error_code code;

        std::filesystem::create_directories(copy.to.parent_path(), code);

        // Skip copies that are already current.
        std::error_code asked;

        const auto size = std::filesystem::file_size(copy.to, asked);

        if (!asked && size == std::filesystem::file_size(copy.from, asked) && !asked
            && std::filesystem::last_write_time(copy.to, asked) >= std::filesystem::last_write_time(copy.from, asked)
            && !asked) {
            continue;
        }

        if (!std::filesystem::copy_file(copy.from, copy.to, std::filesystem::copy_options::overwrite_existing, code)) {
            if (error != nullptr) {
                *error = "DOS cannot open " + copy.from.filename().string()
                         + " by that name, "
                           "and the copy that would carry it in as "
                         + copy.to.filename().string() + " could not be written: " + code.message();
            }

            return false;
        }
    }

    DosFiles::prune(command.where, command.staged);

    // The first run starts from the config in the port folder, which holds what SETUP chose for sound.
    if (const std::filesystem::path own = DosFiles::configFile(config, config.activeProfile()); !own.empty()) {
        std::error_code code;
        const std::filesystem::path setup = DosFiles::inFolder(command.portDirectory, own.filename().string());

        if (!std::filesystem::exists(own, code) && std::filesystem::is_regular_file(setup, code)) {
            std::filesystem::copy_file(setup, own.parent_path() / setup.filename(), code);
        }
    }

    if (!command.responseFile.empty()) {
        if (std::ofstream file(command.responseFile, std::ios::trunc); !(file << command.responseText << "\n")) {
            if (error != nullptr) {
                *error = "This launch is longer than DOS can take at once, and " + command.responseFile.string()
                         + ", which would carry the rest of "
                           "it, could not be written.";
            }

            return false;
        }
    }

    return Launcher::spawn(
            config, command.program, command.arguments, command.portDirectory, {}, true, id, output, error);
}

int spent(const Command &command) {
    return static_cast<int>(std::ranges::count(command.arguments, "-c"));
}

Command command(const Config &config, std::string *error) {
    Built built;

    if (!build(config, built, error)) {
        return {};
    }

    return {
            .program = std::move(built.program),
            .arguments = std::move(built.arguments),
            .notes = std::move(built.notes),
    };
}

}
