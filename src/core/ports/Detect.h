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
#include <string>
#include <string_view>

namespace Detect {

struct Found {
    std::string portId;
    std::string name;
    std::filesystem::path program;
    bool dos{false};
};

[[nodiscard]] bool same(const std::filesystem::path &left, const std::filesystem::path &right);

[[nodiscard]] std::filesystem::path onPath(std::string_view name);

// Looked up once per run.
[[nodiscard]] const std::filesystem::path &dosbox();

// Excludes what ZDL unpacked into its own directory.
[[nodiscard]] std::span<const Found> ports();

[[nodiscard]] const Found *of(const std::filesystem::path &file);

}
