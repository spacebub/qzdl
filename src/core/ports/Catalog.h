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

namespace Catalog {

struct Port {
    std::string_view id;
    std::string_view name;
    std::string_view blurb;
    std::string_view homepage;

    // GitHub repository whose latest release is fetched; empty for a fixed file and version.
    std::string_view repository;
    std::string_view file;
    std::string_view version;

    // '+' separated tokens that must all appear in the name, the last at its end. Empty is no build.
    std::string_view windowsBuild;
    std::string_view linuxBuild;

    // Without suffix.
    std::string_view program;

    bool dos;
};

[[nodiscard]] std::span<const Port> ports();

[[nodiscard]] const Port *find(std::string_view id);

// The build pattern for the platform this was compiled for.
[[nodiscard]] std::string_view pattern(const Port &port);

[[nodiscard]] bool matches(std::string_view name, std::string_view pattern);

[[nodiscard]] std::filesystem::path directory();

[[nodiscard]] std::filesystem::path directory(const Port &port);

[[nodiscard]] std::filesystem::path downloads();

[[nodiscard]] bool runnable(const std::filesystem::path &file, bool dos = false);

// A DOS port is an .exe on every system.
[[nodiscard]] std::filesystem::path program(const std::filesystem::path &directory,
                                            std::string_view name, bool dos = false);

}
