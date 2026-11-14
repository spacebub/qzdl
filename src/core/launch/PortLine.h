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

#include <cstddef>
#include <cstdint>

// The ports the flag and switch tables tell apart. A fork that reads what its parent reads shares its line.
enum class PortLine : std::uint8_t {
    none,
    uzdoom,
    gzdoom,
    zdoom,
    zandronum,
    eternity,
    dsda,
    nyan,
    prboom,
    woof,
    chocolate,
    crispy,
    helion,
};

inline constexpr std::size_t PORT_LINES = static_cast<std::size_t>(PortLine::helion) + 1;

// Several lines at once, one bit each.
using PortLines = std::uint32_t;

[[nodiscard]] constexpr PortLines bitOf(const PortLine line) {
    return line == PortLine::none ? 0U : 1U << (static_cast<unsigned>(line) - 1U);
}
