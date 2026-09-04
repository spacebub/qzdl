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
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "core/Config.h"
#include "core/Process.h"
#include "core/Profile.h"

namespace Launcher {

[[nodiscard]] std::filesystem::path executable(const Config &config);

[[nodiscard]] std::vector<std::string> arguments(const Config &config);

[[nodiscard]] std::filesystem::path getConfigPath(const Profile &profile);
[[nodiscard]] std::filesystem::path getConfigPath(const Config &config);

[[nodiscard]] std::string commandLine(const Config &config);

// The id is the hold on the game that comes back, for asking later whether it
// is still up; the stream is its output, and is only piped when asked for.
bool launch(const Config &config,
            Process::Id *id = nullptr,
            Process::Stream *output = nullptr,
            std::string *error = nullptr);

[[nodiscard]] std::vector<std::string> maps(const Config &config);

}
