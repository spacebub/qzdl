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
#pragma once

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "core/launch/Arguments.h"
#include "core/ports/Flatpak.h"

namespace FlatpakRun {

// What the launcher works out for a port that is a Flatpak export.
struct Launch {
    std::filesystem::path file;
    std::vector<Arguments::Reached> reached;

    // Empty when the port runs in the folder of the export.
    std::filesystem::path runDirectory;

    // The folder of the active IWAD, or empty. A port loads companions such as strife1.wad from it through DOOMWADDIR.
    std::filesystem::path gameFolder;

    // Set with --env, since the metadata of an app can override what flatpak inherits.
    std::vector<std::pair<std::string, std::string>> variables;
};

// flatpak and the words up to the app. Empty arguments mean the launch cannot be built.
struct Command {
    std::filesystem::path flatpak;
    std::vector<std::string> arguments;
};

#ifdef __linux__

// Each path once, and writing wins over reading. A path goes when a folder holds it and grants as much. Nothing at or
// inside home is granted, since a grant there would lay a read-only copy over the app home. Nor is a relative path.
[[nodiscard]] std::vector<Arguments::Reached> grants(const std::vector<Arguments::Reached> &reached,
                                                     const std::filesystem::path &home = {});

[[nodiscard]] std::string grant(const Arguments::Reached &reached);

// The words that pick the installation, branch and arch of a deploy.
[[nodiscard]] std::vector<std::string> installationArguments(const Flatpak::Deploy &deployed);

// flatpak run on the installation, branch and arch the export runs, with the paths the launch reaches, DOOMWADDIR and
// the variables. Then the command of the app and the app. The port arguments follow it.
[[nodiscard]] Command command(Launch launch, std::string *error = nullptr);

#else

// Flatpak exists only on Linux, so no port is one.
[[nodiscard]] inline Command command(const Launch & /*launch*/, std::string * /*error*/ = nullptr) {
    return {};
}

#endif

}
