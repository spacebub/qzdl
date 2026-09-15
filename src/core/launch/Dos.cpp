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

#include "core/config/Schema.h"
#include "core/launch/Arguments.h"
#include "core/launch/Dialect.h"
#include "core/launch/Dos.h"
#include "core/launch/DosFiles.h"
#include "core/launch/Launcher.h"
#include "core/launch/Storage.h"
#include "core/util/Text.h"

namespace Dos {

namespace {

// C: is the port's own directory, which is where DOSBox lands. Mounts share the eleven
// -c commands DOSBox honours with the drive change and the run, plus the exit and
// DOOMWADDIR when they are wanted.
constexpr int FIXED_COMMANDS = 2;
constexpr char FIRST_DRIVE = 'c';
constexpr char LAST_DRIVE = 'j';

// Past this the arguments go into a response file, read with @.
constexpr size_t DOS_LINE_LIMIT = 126;

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
    void within(const std::filesystem::path &directory, const std::filesystem::path &parent,
                const std::string &name) {
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

    [[nodiscard]] const std::vector<std::pair<char, std::string>> &mounts() const {
        return _mounts;
    }

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
    return value.contains(' ') ? "\"" + value + "\"" : value;
}

bool isBatch(const std::filesystem::path &program) {
    const std::string extension = Text::lower(program.extension().string());

    return extension == ".bat" || extension == ".cmd";
}

struct Built {
    std::vector<std::string> arguments;
    std::filesystem::path portDirectory;
    DosFiles::Directories where;
    std::vector<DosFiles::Copy> staged;
    std::filesystem::path responseFile;
    std::string responseText;
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
    const std::filesystem::path port = std::filesystem::absolute(Launcher::executable(config), code);
    const std::string iwad = config.activeIwadFile();

    if (!DosFiles::spellable(port.filename().string())) {
        if (error != nullptr) {
            *error = "DOSBox renames " + port.filename().string() + " on the way in, and "
                "then there is nothing there by that name to run. Eight characters and "
                "three is all DOS can spell.";
        }

        return false;
    }

    const Dialect::Port speaks = Dialect::of(config);

    out.portDirectory = port.parent_path();
    out.where = DosFiles::directories(config, port);

    // A pre-Boom port takes no -iwad: it searches $DOOMWADDIR, or its own directory.
    const DosFiles::Reach game = !speaks.iwad && !iwad.empty()
        ? DosFiles::reach(std::filesystem::absolute(iwad, code), out.portDirectory,
                          speaks.recognised)
        : DosFiles::Reach::beside;

    const bool points = game != DosFiles::Reach::beside;

    DosDrives drives(out.portDirectory, COMMANDS - FIXED_COMMANDS - (points ? 1 : 0)
                                            - (profile.dosExit ? 1 : 0));

    drives.within(out.where.files, out.where.instance, ConfigFile::DOS_FILES_DIR);

    if (const std::filesystem::path replays = Storage::replayDirectory(config); !replays.empty()) {
        drives.within(replays, replays.parent_path(), replays.filename().string());
    }

    DosFiles::Staging staging(out.where);

    std::string wadDirectory;

    // The recording does not exist yet, so the loop below cannot stat it.
    const std::filesystem::path recording = profile.replay.mode == ReplayMode::Record
        ? Storage::replayFile(config)
        : std::filesystem::path();
    const std::string recorded = recording.empty()
        ? std::string()
        : (recording.parent_path() / recording.stem()).string();

    // Staged first so nothing else takes its name.
    if (game == DosFiles::Reach::staged) {
        staging.game(std::filesystem::absolute(iwad, code));
        wadDirectory = drives.spellDirectory(out.where.instance);
    } else if (game == DosFiles::Reach::pointed) {
        wadDirectory = drives.spellDirectory(std::filesystem::absolute(iwad, code).parent_path());
    }

    if (points && wadDirectory.empty()) {
        if (error != nullptr) {
            *error = TOO_MANY_DIRECTORIES;
        }

        return false;
    }

    std::vector<std::string> line;

    // A batch file run bare takes the shell with it, and nothing after it runs.
    line.push_back(isBatch(port) ? "call " + port.filename().string() : port.filename().string());

    // The config does not exist before the first launch, so the loop below cannot stat it.
    const std::string configured = Storage::configFile(config).string();

    for (const std::string &argument : Arguments::of(config, out.portDirectory)) {
        const bool records = !recorded.empty() && argument == recorded;
        const bool configures = !configured.empty() && argument == configured;

        // One directory_entry answers both questions off one stat. This loop runs
        // over every argument, and the preview builds the line twice.
        const std::filesystem::directory_entry entry(argument, code);

        if (!records && !configures && entry.is_directory(code)) {
            if (error != nullptr) {
                *error = "A DOS port cannot load a folder. " + argument + " would have to be "
                    "a WAD or a PK3 for this profile to launch.";
            }

            return false;
        }

        if (!records && !configures && !entry.is_regular_file(code)) {
            line.push_back(argument);
            continue;
        }

        if (records && !DosFiles::spellable(recording.filename().string())) {
            if (error != nullptr) {
                *error = "DOS cannot spell " + recording.filename().string() + ", so there "
                    "would be nothing by that name to record into. Eight characters and "
                    "three is all it can spell.";
            }

            return false;
        }

        std::string spelled;

        if (records) {
            spelled = drives.spell(recording.parent_path() / recording.stem());
        } else if (configures) {
            spelled = drives.spell(configured);
        } else {
            spelled = drives.spell(staging.spellableName(std::filesystem::absolute(argument, code)));
        }

        if (spelled.empty()) {
            if (error != nullptr) {
                *error = TOO_MANY_DIRECTORIES;
            }

            return false;
        }

        line.push_back(std::move(spelled));
    }

    out.staged = staging.planned();

    std::string tail = Text::join({line.begin() + 1, line.end()}, " ");

    if (tail.size() > DOS_LINE_LIMIT) {
        out.responseFile = out.where.instance / "zdl.rsp";
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

    out.arguments.emplace_back("-c");
    out.arguments.push_back(std::string(1, FIRST_DRIVE) + ":");
    out.arguments.emplace_back("-c");
    out.arguments.push_back(tail.empty() ? line.front() : line.front() + " " + tail);

    // -exit closes DOSBox only for a program named as a bare argument, never for one
    // run with -c, so the shell is told to quit instead.
    if (profile.dosExit) {
        out.arguments.emplace_back("-c");
        out.arguments.emplace_back("exit");
    }

    if (profile.dosFullscreen) {
        out.arguments.emplace_back("-fullscreen");
    }

    return true;
}

}

bool start(const Config &config, Process::Id *id, Process::Stream *output,
           std::string *error) {
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

    DosFiles::prune(command.where, command.staged);

    if (!command.responseFile.empty()) {
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

    return Process::start(Launcher::dosbox(config), command.arguments,
                          command.portDirectory, {}, id, output, error);
}

int spent(const Command &command) {
    return static_cast<int>(std::ranges::count(command.arguments, "-c"));
}

Command command(const Config &config, std::string *error) {
    Built built;

    if (!build(config, built, error)) {
        return {};
    }

    return {.dosbox = Launcher::dosbox(config), .arguments = std::move(built.arguments)};
}

}
