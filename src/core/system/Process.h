/*
 * This file is part of qZDL
 * Copyright (C) 2026  spacebub
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

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace Process {

// Zero means no process.
using Id = std::uint64_t;

// Read end of a child's output: a descriptor, or a handle on Windows.
using Stream = std::intptr_t;

constexpr Stream NOTHING = -1;

enum class State : std::uint8_t {
    // Not started, or already reaped.
    Unknown,
    Running,
    Finished,

    // Non-zero status, or a signal.
    Failed,
};

// Starts the program in its own session so it outlives ZDL; without an id it is not tracked.
// Requested output must be drained or the child blocks on a full pipe. A pty is used where
// available so the child's stdio stays line buffered.
bool start(const std::filesystem::path &program,
           const std::vector<std::string> &arguments,
           const std::filesystem::path &workingDirectory,
           const std::map<std::string, std::string> &environment = {},
           Id *id = nullptr,
           Stream *output = nullptr,
           std::string *error = nullptr);

// Non-blocking; false at end of stream.
bool read(Stream output, std::string &into);

// Graceful; the child may save on the way out.
void stop(Id id);

// Immediate, for a child that ignores stop().
void force(Id id);

void closeStream(Stream output);

// Non-blocking. A finished child is reaped and reported once, Unknown after that.
// The code is the exit status, or the negated signal.
State poll(Id id, int *code = nullptr);

}
