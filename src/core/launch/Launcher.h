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
#include <vector>

#include "ttk/system/Process.h"

#include "core/config/Config.h"

namespace Launcher {

[[nodiscard]] std::filesystem::path executable(const Config &config);

[[nodiscard]] bool isDosPort(const Config &config);

[[nodiscard]] std::filesystem::path dosbox(const Config &config);

// Whether the launch hands the port a pipe for its output.
[[nodiscard]] bool captures(const Config &config);

// What a variable is set to: the text without one pair of quotes around all of it, otherwise as typed.
[[nodiscard]] std::string value(const std::string &typed);

// Starts program with the profile's variables set over environment, behind its prefix when prefixed.
bool spawn(const Config &config,
           const std::filesystem::path &program,
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
