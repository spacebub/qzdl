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

#include "core/launch/Port.h"

namespace Catalog {

struct Download {
    // Names the port in zdl.json and in the list, and the folder it unpacks into.
    Port::Port port;

    std::string_view blurb;
    std::string_view homepage;

    // GitHub repository whose latest release is fetched. Empty for a fixed file and version.
    std::string_view repository;
    std::string_view file;
    std::string_view version;

    // SHA-256 of the fixed file, lowercase hex. A release's comes with its answer.
    std::string_view sha256;

    // '+' separated tokens that must all appear in the name, the last at its end. Empty is no build.
    std::string_view windowsBuild;
    std::string_view linuxBuild;

    // Without suffix.
    std::string_view executable;

    bool dos;
};

[[nodiscard]] std::span<const Download> downloads();

[[nodiscard]] const Download *find(std::string_view id);

// What zdl.json stores for the port. It also names the folder the port unpacks into.
[[nodiscard]] std::string_view id(const Download &download);

[[nodiscard]] std::string_view name(const Download &download);

// The build pattern for the platform this was compiled for.
[[nodiscard]] std::string_view pattern(const Download &download);

[[nodiscard]] bool matches(std::string_view name, std::string_view pattern);

[[nodiscard]] std::filesystem::path directory();

[[nodiscard]] std::filesystem::path directory(const Download &download);

[[nodiscard]] std::filesystem::path downloadDirectory();

[[nodiscard]] bool runnable(const std::filesystem::path &file, bool dos = false);

// A DOS port is an .exe on every system.
[[nodiscard]] std::filesystem::path
file(const std::filesystem::path &directory, std::string_view name, bool dos = false);

}
