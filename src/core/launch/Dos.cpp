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

#include <fstream>
#include <utility>

#include "core/launch/Arguments.h"
#include "core/launch/Dialect.h"
#include "core/launch/Dos.h"
#include "core/launch/DosFiles.h"
#include "core/launch/Launcher.h"
#include "core/launch/Storage.h"
#include "core/util/Text.h"

namespace Dos {

namespace {

// C: is the port's own directory. DOSBox silently drops -c commands past the
// tenth: eight mounts, one drive change, one run.
constexpr char FIRST_DRIVE = 'c';
constexpr char LAST_DRIVE = 'j';

// Past this the arguments go into a response file, read with @.
constexpr size_t DOS_LINE_LIMIT = 126;

class DosDrives {
public:
    explicit DosDrives(const std::filesystem::path &port, const char last = LAST_DRIVE)
        : _last(last) {
        driveFor(port.parent_path());
    }

    std::string spell(const std::filesystem::path &file) {
        const char drive = driveFor(file.parent_path());

        return drive == 0
            ? std::string()
            : std::string(1, drive) + ":\\" + file.filename().string();
    }

    [[nodiscard]] const std::vector<std::pair<char, std::string>> &mounts() const {
        return _mounts;
    }

private:
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

    std::vector<std::pair<char, std::string>> _mounts;
    char _next{FIRST_DRIVE};
    char _last{LAST_DRIVE};
};

std::string dosQuote(const std::string &value) {
    return value.contains(' ') ? "\"" + value + "\"" : value;
}

struct Built {
    std::vector<std::string> arguments;
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

    // A pre-Boom port is pointed at the game via DOOMWADDIR, which costs one -c command.
    const bool pointAtGame = !iwad.empty() && !Dialect::of(port).iwad;

    DosDrives drives(port, pointAtGame ? static_cast<char>(LAST_DRIVE - 1) : LAST_DRIVE);
    DosFiles::Staging staging(DosFiles::directory(config, port));
    std::string wadDrive;

    // The recording does not exist yet, so the loop below cannot stat it.
    const std::filesystem::path recording = config.activeProfile().replay.mode == 1
        ? Storage::replayFile(config)
        : std::filesystem::path();
    const std::string recorded = recording.empty()
        ? std::string()
        : (recording.parent_path() / recording.stem()).string();

    // Staged first so nothing else takes its name. Doom Legacy wants its own wads beside the game.
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

        wadDrive = spelled.substr(0, 2);
    }

    std::vector<std::string> line;

    line.push_back(port.filename().string());

    for (const std::string &argument : Arguments::of(config)) {
        const bool records = !recorded.empty() && argument == recorded;

        if (!records && std::filesystem::is_directory(argument, code)) {
            if (error != nullptr) {
                *error = "A DOS port cannot load a folder. " + argument + " would have to be "
                    "a WAD or a PK3 for this profile to launch.";
            }

            return false;
        }

        if (!records && !std::filesystem::is_regular_file(argument, code)) {
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

        std::string spelled = records
            ? drives.spell(recording.parent_path() / recording.stem())
            : drives.spell(staging.spellableName(std::filesystem::absolute(argument, code)));

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

    if (config.activeProfile().dosFullscreen) {
        out.arguments.emplace_back("-fullscreen");
    }

    out.arguments.emplace_back("-exit");

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

Command command(const Config &config, std::string *error) {
    Built built;

    if (!build(config, built, error)) {
        return {};
    }

    return {.dosbox = Launcher::dosbox(config), .arguments = std::move(built.arguments)};
}

}
