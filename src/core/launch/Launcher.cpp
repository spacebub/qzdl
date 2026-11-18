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
#include <concepts>
#include <iterator>
#include <string_view>
#include <utility>

#include "ttk/system/Env.h"
#include "ttk/system/Text.h"

#include "core/launch/Arguments.h"
#include "core/launch/Command.h"
#include "core/launch/Dialect.h"
#include "core/launch/Dos.h"
#include "core/launch/FlatpakRun.h"
#include "core/launch/Launcher.h"
#include "core/launch/Storage.h"
#include "core/ports/Detect.h"
#include "core/ports/Flatpak.h"

using namespace ttk;

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

    for (const GameEntry &iwad : config.iwads) {
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

std::map<std::string, std::string> gameEnvironment(const Config &config, const std::filesystem::path &first) {
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

void setVariables(std::map<std::string, std::string> &environment, const Profile &profile) {
    for (auto &[name, set] : variables(profile)) {
        environment.insert_or_assign(std::move(name), std::move(set));
    }
}

bool start(const Config &config,
           const std::filesystem::path &file,
           const std::vector<std::string> &arguments,
           const std::filesystem::path &directory,
           const std::map<std::string, std::string> &environment,
           const bool prefixed,
           Process::Id *id,
           Process::Stream *output,
           std::string *error) {
    std::vector<std::string> words =
            prefixed ? Text::parse_arguments(config.activeProfile().prefix) : std::vector<std::string>();

    if (words.empty()) {
        return Process::start(file, arguments, directory, environment, id, output, error);
    }

    std::filesystem::path first(words.front());

    if (!first.has_parent_path()) {
        first = Detect::onPath(words.front());

        if (first.empty()) {
            if (error != nullptr) {
                *error = "The prefix " + words.front() + " is not on the PATH.";
            }

            return false;
        }
    } else {
        // A relative path counts from here, not from where the game runs.
        std::error_code code;

        if (std::filesystem::path full = std::filesystem::absolute(first, code); !code) {
            first = std::move(full);
        }
    }

    words.erase(words.begin());
    words.push_back(file.string());
    words.insert(words.end(), arguments.begin(), arguments.end());

    return Process::start(first, words, directory, environment, id, output, error);
}

bool run(const Config &config,
         const std::filesystem::path &file,
         const std::vector<std::string> &arguments,
         const std::filesystem::path &portDirectory,
         const bool prefixed,
         Process::Id *id,
         Process::Stream *output,
         std::string *error) {
    const std::filesystem::path directory = Storage::runDirectory(config, portDirectory);

    // The sandbox takes the profile variables as --env words, and a Flatpak port gets no search path.
    if (sandboxed(config)) {
        return start(config, file, arguments, directory, {}, prefixed, id, output, error);
    }

    // Away from the port directory, only the search path still reaches what sits beside it.
    const std::filesystem::path search = directory == portDirectory ? std::filesystem::path() : portDirectory;

    return spawn(config, file, arguments, directory, gameEnvironment(config, search), prefixed, id, output, error);
}

std::filesystem::path directoryOf(const std::filesystem::path &port) {
    std::error_code code;
    const std::filesystem::path out = std::filesystem::absolute(port, code).parent_path();

    return code ? port.parent_path() : out;
}

// A DOS port runs in DOSBox, whatever its file is.
// The path test goes first, since it is cheap and almost always false.
bool viaFlatpak(const auto &port, const std::invocable auto &dos) {
    return Flatpak::exported(port) && !dos();
}

bool runsFlatpak(const Config &config, const std::filesystem::path &port) {
    return viaFlatpak(port, [&config] { return isDosPort(config); });
}

std::string value(const std::string &typed) {
    const bool wrapped = typed.size() >= 2 && (typed.front() == '"' || typed.front() == '\'')
                         && typed.find(typed.front(), 1) == typed.size() - 1;

    return wrapped ? typed.substr(1, typed.size() - 2) : typed;
}

std::filesystem::path gameFolder(const Config &config, const std::filesystem::path &portDirectory) {
    const std::filesystem::path iwad = Arguments::beside(config.activeIwadFile(), portDirectory);

    return iwad.is_absolute() ? iwad.parent_path() : std::filesystem::path();
}

FlatpakRun::Command
flatpakRun(const Config &config, const PortPath &port, std::vector<Arguments::Reached> reached, std::string *error) {
    const std::filesystem::path run = Storage::plannedRunDirectory(config, port.directory);

    return FlatpakRun::command(
            {
                    .file = port.file,
                    .reached = std::move(reached),
                    .runDirectory = run == port.directory ? std::filesystem::path() : run,
                    .gameFolder = gameFolder(config, port.directory),
                    .variables = variables(config.activeProfile()),
            },
            error);
}

void makeReplayFolder(const Config &config) {
    const ReplayMode mode = config.activeProfile().replay.mode;

    if (mode == ReplayMode::Record || mode == ReplayMode::Continue) {
        if (const std::filesystem::path replays = Storage::replayDirectory(config); !replays.empty()) {
            std::error_code made;
            std::filesystem::create_directories(replays, made);
        }
    }
}

// Some ports drop -shotdir when the folder is not there yet.
void makeScreenshotFolder(const Config &config) {
    if (const std::filesystem::path shots = Storage::screenshotDirectory(config);
        Dialect::of(config).has(Dialect::Feature::shotdir) && !shots.empty()) {
        std::error_code made;
        std::filesystem::create_directories(shots, made);
    }
}

}

bool spawn(const Config &config,
           const std::filesystem::path &file,
           const std::vector<std::string> &arguments,
           const std::filesystem::path &directory,
           std::map<std::string, std::string> environment,
           const bool prefixed,
           Process::Id *id,
           Process::Stream *output,
           std::string *error) {
    setVariables(environment, config.activeProfile());

    return start(config, file, arguments, directory, environment, prefixed, id, output, error);
}

PortPath portPath(const Config &config) {
    std::filesystem::path file = executable(config);
    std::filesystem::path directory = file.empty() ? std::filesystem::path() : directoryOf(file);

    return {.file = std::move(file), .directory = std::move(directory)};
}

Program program(const Config &config, std::string *error) {
    PortPath port = portPath(config);

    if (!runsFlatpak(config, port.file)) {
        std::vector<std::string> arguments = Arguments::of(config, port.directory);

        return {
                .file = std::move(port.file),
                .arguments = std::move(arguments),
                .portDirectory = std::move(port.directory),
        };
    }

    Arguments::Built built = Arguments::withReached(config, port.directory);
    FlatpakRun::Command run = flatpakRun(config, port, std::move(built.reached), error);

    if (!run.arguments.empty()) {
        run.arguments.insert(run.arguments.end(),
                             std::make_move_iterator(built.arguments.begin()),
                             std::make_move_iterator(built.arguments.end()));
    }

    return {
            .file = std::move(run.flatpak),
            .arguments = std::move(run.arguments),
            .portDirectory = std::move(port.directory),
    };
}

std::vector<std::string>
portRun(const Config &config, const PortPath &port, std::vector<Arguments::Reached> named, std::string *error) {
    if (!runsFlatpak(config, port.file)) {
        return {port.file.string()};
    }

    std::vector<Arguments::Reached> reached = Arguments::withReached(config, port.directory).reached;

    reached.insert(reached.end(), std::make_move_iterator(named.begin()), std::make_move_iterator(named.end()));

    FlatpakRun::Command run = flatpakRun(config, port, std::move(reached), error);

    if (run.arguments.empty()) {
        return {};
    }

    run.arguments.insert(run.arguments.begin(), run.flatpak.string());

    return std::move(run.arguments);
}

std::vector<std::pair<std::string, std::string>> variables(const Profile &profile) {
    std::vector<std::pair<std::string, std::string>> out;

    for (const EnvVariable &variable : profile.environment) {
        if (!variable.name.empty()) {
            out.emplace_back(variable.name, value(variable.value));
        }
    }

    return out;
}

std::filesystem::path executable(const Config &config) {
    const Engine *engine = config.findEngine(config.activeProfile().engine);

    return engine != nullptr ? std::filesystem::path(engine->file) : std::filesystem::path();
}

bool sandboxed(const Config &config) {
    const Profile &profile = config.activeProfile();

    return runsFlatpak(config, executable(config))
           && (!profile.on(Switch::customCommand) || Command::startsPort(profile.command));
}

bool runsFlatpak(const Engine &engine) {
    return viaFlatpak(std::string_view(engine.file),
                      [&engine] { return Dialect::of(engine).has(Dialect::Feature::dos); });
}

bool isDosPort(const Config &config) {
    return Dialect::of(config).has(Dialect::Feature::dos);
}

std::filesystem::path dosbox(const Config &config) {
    return config.general.dosbox.empty() ? Detect::dosbox() : std::filesystem::path(config.general.dosbox);
}

bool captures(const Config &config) {
    return captures(config, Dialect::of(config));
}

bool captures(const Config &config, const Dialect::Traits &traits) {
    // DOSBox prints into its own window, and auto close would leave a pipe nobody drains.
    return config.activeProfile().on(Switch::captureOutput) && !traits.has(Dialect::Feature::dos)
           && !config.general.autoClose;
}

bool launch(const Config &config, Process::Id *id, Process::Stream *output, std::string *error) {
    const Profile &profile = config.activeProfile();
    const std::filesystem::path file = executable(config);

    if (profile.on(Switch::customCommand)) {
        const std::vector<std::string> tokens = Command::custom(config, error);

        if (tokens.empty()) {
            return false;
        }

        makeReplayFolder(config);

        std::filesystem::path typed(tokens.front());

        if (!typed.has_parent_path()) {
            if (std::filesystem::path found = Detect::onPath(tokens.front()); !found.empty()) {
                typed = std::move(found);
            }
        }

        // A relative name counts from here, not from where the game runs.
        if (typed.has_parent_path()) {
            std::error_code code;

            if (std::filesystem::path full = std::filesystem::absolute(typed, code); !code) {
                typed = std::move(full);
            }
        }

        std::error_code made;

        if (Command::uses(profile.command, Command::Placeholder::Profile)
            || Command::uses(profile.command, Command::Placeholder::CfgDir)) {
            std::filesystem::create_directories(Storage::configFile(config).parent_path(), made);
        }

        if (Command::uses(profile.command, Command::Placeholder::SaveDir)) {
            std::filesystem::create_directories(Storage::saveDirectory(config), made);
        }

        if (Command::uses(profile.command, Command::Placeholder::ReplayDir)) {
            std::filesystem::create_directories(Storage::replayDirectory(config), made);
        }

        Storage::adoptConfig(config);

        // A Flatpak port starts in its sandbox, so it runs where the generated launch runs it.
        const std::filesystem::path portDirectory = sandboxed(config) ? directoryOf(file) : typed.parent_path();

        return run(config, typed, {tokens.begin() + 1, tokens.end()}, portDirectory, false, id, output, error);
    }

    if (file.empty()) {
        if (error != nullptr) {
            *error = "No source port is selected.";
        }

        return false;
    }

    if (isDosPort(config)) {
        makeReplayFolder(config);
        makeScreenshotFolder(config);

        return Dos::start(config, id, output, error);
    }

    const Program started = program(config, error);

    if (started.file.empty()) {
        return false;
    }

    makeReplayFolder(config);
    makeScreenshotFolder(config);

    if (const std::filesystem::path own = Storage::configFile(config); !own.empty()) {
        std::error_code made;
        std::filesystem::create_directories(own.parent_path(), made);
        std::filesystem::create_directories(Storage::saveDirectory(config), made);
        Storage::adoptConfig(config);
    }

    std::error_code code;
    std::filesystem::path resolved = std::filesystem::absolute(started.file, code);

    if (code) {
        resolved = started.file;
    }

    return run(config, resolved, started.arguments, started.portDirectory, true, id, output, error);
}

}
