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

#include <cstddef>

#include "gui/draw/Mark.h"

namespace Embedded {

extern const unsigned char mark128[];
extern const std::size_t mark128Size;
extern const unsigned char mark256[];
extern const std::size_t mark256Size;

}

namespace {

BLImage decode(const unsigned char *data, const std::size_t size) {
    BLImage image;

    image.read_from_data(data, size);

    return image;
}

}

const BLImage &Mark::of(const int side) {
    static const BLImage small = decode(Embedded::mark128, Embedded::mark128Size);
    static const BLImage large = decode(Embedded::mark256, Embedded::mark256Size);

    return side > 128 ? large : small;
}
