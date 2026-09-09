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

#include <filesystem>
#include <span>
#include <string_view>

// The source ports ZDL knows where to get, and what to do with one once it is
// here. Nothing in here reaches the network: it is only what to ask for.
namespace Catalog {

struct Port {
    std::string_view id;
    std::string_view name;
    std::string_view blurb;
    std::string_view homepage;

    // The GitHub repository whose latest release is asked about, or nothing
    // when the build never moves and is named outright below.
    std::string_view repository;
    std::string_view file;
    std::string_view version;

    /*
    What a build for each system is called: every token separated by a '+' has
    to be somewhere in the name and the last of them at the end of it. Empty
    is a system this port has no build for.
    */
    std::string_view windowsBuild;
    std::string_view linuxBuild;

    // The program to run once it is unpacked, without a suffix.
    std::string_view program;

    // A DOS program, which runs inside DOSBox wherever it is run.
    bool dos;
};

// The list itself is in the binary rather than built on first use: every
// field of it is a view of text that is there already.
[[nodiscard]] std::span<const Port> ports();

[[nodiscard]] const Port *find(std::string_view id);

// How a build for the system this was compiled for is named.
[[nodiscard]] std::string_view pattern(const Port &port);

[[nodiscard]] bool matches(std::string_view name, std::string_view pattern);

// Where the ports ZDL fetches are kept, and one port's own place in it.
[[nodiscard]] std::filesystem::path directory();

[[nodiscard]] std::filesystem::path directory(const Port &port);

// Where what was downloaded is kept, so that fetching the same build again
// does not bring it down the wire twice.
[[nodiscard]] std::filesystem::path downloads();

[[nodiscard]] bool runnable(const std::filesystem::path &file, bool dos = false);

// The program inside an unpacked port, or nothing when there is none. A DOS
// one is an .exe on every system, since DOSBox is what runs it.
[[nodiscard]] std::filesystem::path program(const std::filesystem::path &directory,
                                            std::string_view name, bool dos = false);

}
