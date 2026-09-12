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

#include <span>
#include <cstdint>
#include <string>
#include <vector>

// On-disk cache of release lookups, so the origin is not hit on every start.
namespace Releases {
struct Answer {
    std::string portId;
    std::string version;
    std::string url;
    std::string asset;

    // The caller's own state value, stored as-is; nothing here reads it.
    std::uint8_t verdict{0};

    std::string note;

    // Seconds since the epoch.
    long long checked{0};
    long long size{0};
};

[[nodiscard]] long long now();

[[nodiscard]] bool fresh(long long checked);

[[nodiscard]] std::vector<Answer> read();
void write(std::span<const Answer> answers);

}
