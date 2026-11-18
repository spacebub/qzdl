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

namespace Port {

// In the order a file name is matched, most specific first.
enum class Port : std::uint8_t {
    unknown,
    doom,
    legacy,
    helion,
    zdoom,
    zandronum,
    skulltag,
    uzdoom,
    gzdoom,
    vkdoom,

    // Any other name with zdoom in it, such as LZDoom or QZDoom. It speaks GZDoom.
    zdoomFork,

    crispyHeretic,
    crispyHexen,
    crispyStrife,
    crispy,
    chocolateHeretic,
    chocolateHexen,
    chocolateStrife,
    chocolate,

    boom,
    mbf,
    eternity,
    woof,
    nugget,
    cherry,
    dsda,
    nyan,
    prboomPlus,
    glboomPlus,
    prboom,
    glboom,
    rude,
};

static_assert(Port{} == Port::unknown, "a port the table does not name is unknown");

}
