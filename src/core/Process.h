/*
 * This file is part of qZDL
 * Copyright (C) 2026  spacebub
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

namespace Process {

/**
 * Starts a program and lets go of it.  ZDL's whole job is to hand a source
 * port a command line and get out of the way, so nothing is kept: no pipes, no
 * exit status, and closing ZDL does not take the game down with it.
 *
 * Returns false and fills error in when the program could not be started at
 * all; a port that starts and then fails on its own is its own business.
 */
bool startDetached(const std::filesystem::path &program,
                   const std::vector<std::string> &arguments,
                   const std::filesystem::path &workingDirectory,
                   std::string *error = nullptr);

}
