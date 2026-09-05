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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace Process {

// A child ZDL started and is still holding on to. Zero is nothing.
using Id = std::uint64_t;

// The reading end of a child's output: a descriptor, or a handle on Windows.
using Stream = std::intptr_t;

constexpr Stream NOTHING = -1;

enum class State : std::uint8_t {
    // Never started under this id, or already reported.
    Unknown,
    Running,
    Finished,

    // A status other than zero, or a signal.
    Failed,
};

/*
Starts the program in a session of its own, so it outlives ZDL either way. The
id is only a hold for asking after it while both are up; nothing here ever
stops a child. Passing no id starts it and forgets it.

Output is only taken when it is asked for, and then whoever asked has to read
it until it ends: a child whose output nobody takes stops as soon as what it is
writing into fills up.

What it is given is a terminal rather than a pipe, where there is one to give.
A C library writing to a pipe holds thousands of characters back before it
sends any, so a game's first words would not arrive until it had said several
pages of them; writing to a terminal it sends every line as it comes, which is
what makes a live view live.
*/
bool start(const std::filesystem::path &program,
           const std::vector<std::string> &arguments,
           const std::filesystem::path &workingDirectory,
           const std::map<std::string, std::string> &environment = {},
           Id *id = nullptr,
           Stream *output = nullptr,
           std::string *error = nullptr);

// Reads what is there without waiting. False is the end of it.
bool read(Stream output, std::string &into);

// Asks the child to quit. It is asked, not killed, so it can save on the way out.
void stop(Id id);

void closeStream(Stream output);

/*
Where the child has got to, without waiting on it. One that has ended is reaped
here, so its end comes back once and Unknown after that. The code is the exit
status, or the negated signal for one that was killed.
*/
State poll(Id id, int *code = nullptr);

}
