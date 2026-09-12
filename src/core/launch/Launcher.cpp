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
#include <utility>

#include "core/launch/Arguments.h"
#include "core/launch/Command.h"
#include "core/launch/Dialect.h"
#include "core/launch/Dos.h"
#include "core/launch/Launcher.h"
#include "core/launch/Storage.h"
#include "core/ports/Detect.h"
#include "core/system/Env.h"

namespace Launcher {

namespace {

#ifdef _WIN32
constexpr char SEPARATOR = ';';
#else
constexpr char SEPARATOR = ':';
#endif

// first, then the directory of every registered IWAD, so a port can find companions such as
// strife1.wad beside sve.wad through its own search path.
std::string wadSearchPath(const Config &config, const std::filesystem::path &first) {
    std::vector<std::string> directories;

    if (!first.empty()) {
        directories.push_back(first.string());
    }

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

    std::string joined = Env::get("DOOMWADPATH");

    for (const std::string &directory : directories) {
        if (!joined.empty()) {
            joined.push_back(SEPARATOR);
        }

        joined.append(directory);
    }

    return joined;
}

std::map<std::string, std::string> gameEnvironment(const Config &config,
                                                   const std::filesystem::path &first) {
    std::map<std::string, std::string> environment;

    if (std::string search = wadSearchPath(config, first); !search.empty()) {
        environment.emplace("DOOMWADPATH", std::move(search));
    }

    // Only DOOMWADDIR resolves a required companion IWAD, so it points at the profile's own.
    if (const std::string iwad = config.activeIwadFile(); !iwad.empty()) {
        std::error_code code;
        if (std::filesystem::path const directory = std::filesystem::path(iwad).parent_path();
            !directory.empty() && std::filesystem::is_directory(directory, code)) {
            environment.insert_or_assign("DOOMWADDIR", directory.string());
        }
    }

    return environment;
}

bool run(const Config &config, const std::filesystem::path &program,
         const std::vector<std::string> &arguments, Process::Id *id, Process::Stream *output,
         std::string *error) {
    const std::filesystem::path programDirectory = program.parent_path();
    const std::filesystem::path directory = Storage::runDirectory(config, programDirectory);

    // Away from the program's directory, only the search path still reaches what sits beside it.
    const std::filesystem::path search =
        directory == programDirectory ? std::filesystem::path() : programDirectory;

    return Process::start(program, arguments, directory, gameEnvironment(config, search), id,
                          output, error);
}

}

std::filesystem::path executable(const Config &config) {
    const NameEntry *port = config.findPort(config.activeProfile().port);

    return port != nullptr ? std::filesystem::path(port->file) : std::filesystem::path();
}

bool isDosPort(const Config &config) {
    return Dialect::of(config).dos;
}

std::filesystem::path dosbox(const Config &config) {
    return config.general.dosbox.empty()
        ? Detect::dosbox()
        : std::filesystem::path(config.general.dosbox);
}

bool launch(const Config &config, Process::Id *id, Process::Stream *output, std::string *error) {
    const Profile &profile = config.activeProfile();
    const std::filesystem::path port = executable(config);

    if (profile.replay.mode == 1) {
        if (const std::filesystem::path replays = Storage::replayDirectory(config); !replays.empty()) {
            std::error_code made;
            std::filesystem::create_directories(replays, made);
        }
    }

    if (profile.customCommand) {
        const std::vector<std::string> tokens = Command::custom(config, error);

        if (tokens.empty()) {
            return false;
        }

        std::filesystem::path program(tokens.front());

        if (!program.has_parent_path()) {
            if (std::filesystem::path found = Detect::onPath(tokens.front()); !found.empty()) {
                program = std::move(found);
            }
        }

        // A relative name counts from here, not from where the game runs.
        if (program.has_parent_path()) {
            std::error_code code;

            if (std::filesystem::path full = std::filesystem::absolute(program, code); !code) {
                program = std::move(full);
            }
        }

        std::error_code made;

        if (profile.command.contains("{profile}") || profile.command.contains("{cfgdir}")) {
            std::filesystem::create_directories(Storage::configFile(config).parent_path(), made);
        }

        if (profile.command.contains("{savedir}")) {
            std::filesystem::create_directories(Storage::saveDirectory(config), made);
        }

        if (profile.command.contains("{replaydir}")) {
            std::filesystem::create_directories(Storage::replayDirectory(config), made);
        }

        return run(config, program, {tokens.begin() + 1, tokens.end()}, id, output, error);
    }

    if (port.empty()) {
        if (error != nullptr) {
            *error = "No source port is selected.";
        }

        return false;
    }

    if (isDosPort(config)) {
        return Dos::start(config, id, output, error);
    }

    if (const std::filesystem::path own = Storage::configFile(config); !own.empty()) {
        std::error_code made;
        std::filesystem::create_directories(own.parent_path(), made);
        std::filesystem::create_directories(Storage::saveDirectory(config), made);
    }

    std::error_code code;
    std::filesystem::path resolved = std::filesystem::absolute(port, code);

    if (code) {
        resolved = port;
    }

    return run(config, resolved, Arguments::of(config, resolved.parent_path()), id, output, error);
}

}
