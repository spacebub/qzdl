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
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "ttk/system/Process.h"

#include "core/config/Config.h"
#include "core/launch/Arguments.h"
#include "core/launch/Dialect.h"

namespace Launcher {

[[nodiscard]] std::filesystem::path executable(const Config &config);

// The launch runs the port through flatpak. The sandbox then gets the profile variables and the process gets none.
[[nodiscard]] bool sandboxed(const Config &config);

[[nodiscard]] bool isDosPort(const Config &config);

// Whether the engine runs through flatpak: an export that does not run in DOSBox. Reads no disk.
[[nodiscard]] bool runsFlatpak(const Engine &engine);

[[nodiscard]] std::filesystem::path dosbox(const Config &config);

// Whether the launch hands the port a pipe for its output.
[[nodiscard]] bool captures(const Config &config);

// For a caller that already holds the traits of the active engine.
[[nodiscard]] bool captures(const Config &config, const Dialect::Traits &traits);

// What a generated launch outside DOSBox starts behind the prefix.
struct Program {
    // Empty for a profile without a port, or when the port cannot run.
    std::filesystem::path file;

    std::vector<std::string> arguments;

    // Where the port sits. A Flatpak runs through flatpak from elsewhere.
    std::filesystem::path portDirectory;

    Arguments::Notes notes;
};

// The one source of what runs, for the launch and for the line that shows it. The error says why the port cannot run.
[[nodiscard]] Program program(const Config &config, std::string *error = nullptr);

struct PortPath {
    std::filesystem::path file;

    // Where the generated launch finds relative files.
    std::filesystem::path directory;
};

// Empty for a profile without a port.
[[nodiscard]] PortPath portPath(const Config &config);

// What a whole {source_port} in a custom command runs: the port file, or for a Flatpak export the flatpak words of
// program() up to the app. These grant what the generated launch reaches and what named adds. Empty when the port
// cannot run.
[[nodiscard]] std::vector<std::string> portRun(const Config &config,
                                               const PortPath &port,
                                               std::vector<Arguments::Reached> named,
                                               std::string *error = nullptr);

// Each variable is set to the text without one pair of quotes around all of it, otherwise as typed.
[[nodiscard]] std::vector<std::pair<std::string, std::string>> variables(const Profile &profile);

// Runs `file` behind the prefix when prefixed. The variables of the profile are set over `environment`.
bool spawn(const Config &config,
           const std::filesystem::path &file,
           const std::vector<std::string> &arguments,
           const std::filesystem::path &directory,
           std::map<std::string, std::string> environment,
           bool prefixed,
           ttk::Process::Id *id,
           ttk::Process::Stream *output,
           std::string *error);

bool launch(const Config &config,
            ttk::Process::Id *id = nullptr,
            ttk::Process::Stream *output = nullptr,
            std::string *error = nullptr);

}
