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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "core/Config.h"

/**
 * Turns a config into a source port command line, and runs it.  Everything the
 * interface has been collecting comes down to this.
 */
namespace Launcher {

/** The source port executable the active profile names, or empty when none. */
[[nodiscard]] std::filesystem::path executable(const Config &config);

/** Every argument the active profile works out to, in the order they go. */
[[nodiscard]] std::vector<std::string> arguments(const Config &config);

/** The same thing as one line, quoted, for showing and copying. */
[[nodiscard]] std::string commandLine(const Config &config);

/**
 * Starts the port in its own directory and lets go of it.  Returns false with
 * a reason when there is no port to start or it could not be started.
 */
bool launch(const Config &config, std::string *error = nullptr);

/**
 * Every map name reachable from the active profile: the IWAD's own, plus those
 * of every external file that is switched on.  Sorted naturally, no duplicates.
 */
[[nodiscard]] std::vector<std::string> maps(const Config &config);

}
