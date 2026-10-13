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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <blend2d/blend2d.h>

#include "support/Canvas.h"

namespace draw {

inline int channel(const std::uint32_t pixel, const unsigned shift) {
    return static_cast<int>((pixel >> shift) & 0xffU);
}

// The largest difference across red, green and blue.
inline int distance(const std::uint32_t one, const std::uint32_t two) {
    int widest = 0;

    for (const unsigned shift : {0U, 8U, 16U}) {
        widest = std::max(widest, std::abs(channel(one, shift) - channel(two, shift)));
    }

    return widest;
}

// The largest difference across every channel, alpha too.
inline int apart(const std::uint32_t one, const std::uint32_t two) {
    return std::max(distance(one, two), std::abs(channel(one, 24) - channel(two, 24)));
}

inline bool leans(const std::uint32_t pixel, const BLRgba32 ink, const BLRgba32 ground) {
    return distance(pixel, ink.value) < distance(pixel, ground.value);
}

struct Ink {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    std::size_t pixels = 0;
};

// The bounds of every pixel in `region` that differs from `ground`.
inline Ink ink(const tests::Canvas &canvas, const BLRectI &region, const std::uint32_t ground) {
    Ink found{.left = region.x + region.w, .top = region.y + region.h, .right = region.x, .bottom = region.y};

    for (int y = region.y; y < region.y + region.h; ++y) {
        for (int x = region.x; x < region.x + region.w; ++x) {
            if (canvas.at(x, y) != ground) {
                found.left = std::min(found.left, x);
                found.top = std::min(found.top, y);
                found.right = std::max(found.right, x + 1);
                found.bottom = std::max(found.bottom, y + 1);
                ++found.pixels;
            }
        }
    }

    return found;
}

inline int count_near(const tests::Canvas &canvas, const BLRectI &area, const BLRgba32 ink, const int slack = 24) {
    int found = 0;

    for (int y = area.y; y < area.y + area.h; ++y) {
        for (int x = area.x; x < area.x + area.w; ++x) {
            if (distance(canvas.at(x, y), ink.value) <= slack) {
                ++found;
            }
        }
    }

    return found;
}

// One pixel of an image, as 0xAARRGGBB premultiplied. Outside it is 0.
inline std::uint32_t texel(const BLImage &image, const int x, const int y) {
    BLImageData data{};

    if (x < 0 || y < 0 || x >= image.width() || y >= image.height() || image.get_data(&data) != BL_SUCCESS) {
        return 0;
    }

    std::uint32_t pixel = 0;

    std::memcpy(&pixel,
                static_cast<const std::uint8_t *>(data.pixel_data) + (static_cast<std::ptrdiff_t>(y) * data.stride)
                        + (static_cast<std::ptrdiff_t>(x) * 4),
                4);

    return pixel;
}

inline void put(BLImage &image, const int x, const int y, const std::uint32_t pixel) {
    BLImageData data{};

    if (image.make_mutable(&data) != BL_SUCCESS) {
        return;
    }

    std::memcpy(static_cast<std::uint8_t *>(data.pixel_data) + (static_cast<std::ptrdiff_t>(y) * data.stride)
                        + (static_cast<std::ptrdiff_t>(x) * 4),
                &pixel,
                4);
}

}
