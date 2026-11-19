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

// The ports the flag tables tell apart. A fork that reads what its parent reads shares its line.
enum class PortLine : std::uint8_t {
    none,
    uzdoom,
    gzdoom,
    zdoom28,
    zandronum32,
    skulltag,
    eternity,
    dsda,
    nyan,
    prboom,
    woof,
    chocolate,
    crispy,
    helion,

    // Older releases whose flag table differs from the newest: they lack bits, or read some under another name or
    // meaning. ZDoom 2.6.0 to 2.7.1, ZDoom 2.5.0 and earlier, Zandronum 3.1 and Zandronum 3.0 to 3.0.1.
    zdoom26,
    zdoom25,
    zandronum31,
    zandronum30,
};

inline constexpr std::size_t PORT_LINES = static_cast<std::size_t>(PortLine::zandronum30) + 1;

// Several lines at once, one bit each.
using PortLines = std::uint32_t;

[[nodiscard]] constexpr PortLines bitOf(const PortLine line) {
    return line == PortLine::none ? 0U : 1U << (static_cast<unsigned>(line) - 1U);
}

[[nodiscard]] constexpr bool has(const PortLines lines, const PortLine line) {
    return (lines & bitOf(line)) != 0;
}

// The lines the flag tables name, alone and together.
namespace Lines {
inline constexpr PortLines UZDOOM = bitOf(PortLine::uzdoom);
inline constexpr PortLines GZDOOM = bitOf(PortLine::gzdoom);

// Each release of ZDoom and Zandronum from the one named on, or up to it.
inline constexpr PortLines ZDOOM_FROM_2_8 = bitOf(PortLine::zdoom28);
inline constexpr PortLines ZDOOM_FROM_2_6 = ZDOOM_FROM_2_8 | bitOf(PortLine::zdoom26);
inline constexpr PortLines ZDOOM_TO_2_5 = bitOf(PortLine::zdoom25);
inline constexpr PortLines ZDOOM = ZDOOM_FROM_2_6 | ZDOOM_TO_2_5;
inline constexpr PortLines ZDOOM_TO_2_7 = ZDOOM & ~ZDOOM_FROM_2_8;
inline constexpr PortLines ZANDRONUM_FROM_3_2 = bitOf(PortLine::zandronum32);
inline constexpr PortLines ZANDRONUM_FROM_3_1 = ZANDRONUM_FROM_3_2 | bitOf(PortLine::zandronum31);
inline constexpr PortLines ZANDRONUM_3_0 = bitOf(PortLine::zandronum30);
inline constexpr PortLines ZANDRONUM = ZANDRONUM_FROM_3_1 | ZANDRONUM_3_0;
inline constexpr PortLines SKULLTAG = bitOf(PortLine::skulltag);
inline constexpr PortLines ETERNITY = bitOf(PortLine::eternity);

// GZDoom and its fork UZDoom.
inline constexpr PortLines GZDOOM_LINE = UZDOOM | GZDOOM;

// Without Zandronum, which forked from ZDoom 2.8 and gave some bits other meanings.
inline constexpr PortLines ZDOOM_LINE = GZDOOM_LINE | ZDOOM;
inline constexpr PortLines ZDOOM_FAMILY = ZDOOM_LINE | ZANDRONUM;

// The same without the ZDoom releases before the one named.
inline constexpr PortLines ZDOOM_LINE_FROM_2_8 = GZDOOM_LINE | ZDOOM_FROM_2_8;
inline constexpr PortLines ZDOOM_LINE_FROM_2_6 = GZDOOM_LINE | ZDOOM_FROM_2_6;
inline constexpr PortLines ZDOOM_FAMILY_FROM_2_6 = ZDOOM_LINE_FROM_2_6 | ZANDRONUM;

// Skulltag and Zandronum, which grew from it.
inline constexpr PortLines SKULLTAG_LINE = SKULLTAG | ZANDRONUM;

// The family with Skulltag, for the bits Skulltag reads the same way.
inline constexpr PortLines EVERY_ZDOOM = ZDOOM_FAMILY | SKULLTAG;
}
