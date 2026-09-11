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

#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>
#include <vector>

#include "gui/draw/Paint.h"

namespace {

// How many blurred sprites are kept before the lot is thrown away.
constexpr size_t KEPT = 24;

// A CSS blur radius is two standard deviations.
double sigmaOf(const double blur) {
    return blur * 0.5;
}

// Three box passes approximate a Gaussian closely enough that nothing here can
// tell the difference; this is the usual width for one of them.
int boxOf(const double blur) {
    const int width = static_cast<int>(std::lround(sigmaOf(blur) * 1.88));

    return std::max(1, width);
}

// One horizontal box pass over premultiplied BGRA, using a running sum so the cost
// does not depend on the radius. Vertical is the same pass over a transposed copy,
// which keeps this to one loop rather than two nearly identical ones.
void blurRows(std::vector<uint32_t> &pixels, const int width, const int height,
              const int radius) {
    if (radius < 1) {
        return;
    }

    const int span = (radius * 2) + 1;
    std::vector<uint32_t> row(static_cast<size_t>(width));

    for (int y = 0; y < height; ++y) {
        uint32_t *line = pixels.data() + (static_cast<size_t>(y) * width);

        std::copy_n(line, width, row.begin());

        int blue = 0;
        int green = 0;
        int red = 0;
        int alpha = 0;

        // The window starts hanging off the left edge, where every sample is the
        // first pixel -- the sprite is transparent there, so this is also zero.
        for (int at = -radius; at <= radius; ++at) {
            const uint32_t pixel = row[static_cast<size_t>(std::clamp(at, 0, width - 1))];

            blue += static_cast<int>(pixel & 0xff);
            green += static_cast<int>((pixel >> 8) & 0xff);
            red += static_cast<int>((pixel >> 16) & 0xff);
            alpha += static_cast<int>((pixel >> 24) & 0xff);
        }

        for (int x = 0; x < width; ++x) {
            line[x] = static_cast<uint32_t>(blue / span)
                      | (static_cast<uint32_t>(green / span) << 8)
                      | (static_cast<uint32_t>(red / span) << 16)
                      | (static_cast<uint32_t>(alpha / span) << 24);

            const uint32_t leaving = row[static_cast<size_t>(std::clamp(x - radius, 0, width - 1))];
            const uint32_t joining =
                row[static_cast<size_t>(std::clamp(x + radius + 1, 0, width - 1))];

            blue += static_cast<int>(joining & 0xff) - static_cast<int>(leaving & 0xff);
            green += static_cast<int>((joining >> 8) & 0xff)
                     - static_cast<int>((leaving >> 8) & 0xff);
            red += static_cast<int>((joining >> 16) & 0xff)
                   - static_cast<int>((leaving >> 16) & 0xff);
            alpha += static_cast<int>((joining >> 24) & 0xff)
                     - static_cast<int>((leaving >> 24) & 0xff);
        }
    }
}

void transpose(const std::vector<uint32_t> &from, std::vector<uint32_t> &to, const int width,
               const int height) {
    to.resize(from.size());

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            to[(static_cast<size_t>(x) * height) + y] = from[(static_cast<size_t>(y) * width) + x];
        }
    }
}

using Shape = std::tuple<int, int, int, int, uint32_t>;

std::map<Shape, BLImage> sprites;

}

double Paint::bleed(const double blur) {
    // Three box passes reach one and a half box widths, near three sigma; short of
    // that the tail is cut off at the sprite's border and reads as an edge.
    return std::ceil(sigmaOf(blur) * 3.0) + 2.0;
}

const BLImage &Paint::shadow(const int width, const int height, const double radius,
                             const double blur, const BLRgba32 tint) {
    const Shape shape{width, height, static_cast<int>(std::lround(radius * 4.0)),
                      static_cast<int>(std::lround(blur * 4.0)), tint.value};

    const auto found = sprites.find(shape);

    if (found != sprites.end()) {
        return found->second;
    }

    // A resize walks the card through a new size every frame, and each is a shape
    // of its own; without a ceiling the cache would grow for as long as the drag.
    if (sprites.size() > KEPT) {
        sprites.clear();
    }

    const int pad = static_cast<int>(bleed(blur));
    const int across = width + (pad * 2);
    const int down = height + (pad * 2);

    BLImage sprite;

    if (sprite.create(across, down, BL_FORMAT_PRGB32) != BL_SUCCESS) {
        return sprites[shape];
    }

    {
        BLContext context(sprite);

        context.clear_all();
        context.fill_round_rect(BLRect{static_cast<double>(pad), static_cast<double>(pad),
                                       static_cast<double>(width), static_cast<double>(height)},
                                radius, radius, tint);
    }

    BLImageData data{};

    if (sprite.make_mutable(&data) != BL_SUCCESS) {
        return sprites[shape];
    }

    // Blend2D rows may be padded, so the blur works on a packed copy.
    std::vector<uint32_t> pixels(static_cast<size_t>(across) * down);

    for (int y = 0; y < down; ++y) {
        const auto *line = reinterpret_cast<const uint32_t *>(
            static_cast<const uint8_t *>(data.pixel_data) + (static_cast<ptrdiff_t>(y) * data.stride));

        std::copy_n(line, across, pixels.begin() + static_cast<ptrdiff_t>(y) * across);
    }

    const int box = boxOf(blur);
    std::vector<uint32_t> turned;

    for (int pass = 0; pass < 3; ++pass) {
        blurRows(pixels, across, down, box);

        transpose(pixels, turned, across, down);
        blurRows(turned, down, across, box);
        transpose(turned, pixels, down, across);
    }

    for (int y = 0; y < down; ++y) {
        auto *line = reinterpret_cast<uint32_t *>(static_cast<uint8_t *>(data.pixel_data)
                                                  + (static_cast<ptrdiff_t>(y) * data.stride));

        std::copy_n(pixels.begin() + static_cast<ptrdiff_t>(y) * across, across, line);
    }

    return sprites.emplace(shape, std::move(sprite)).first->second;
}

BLGradient Paint::down(const BLRect &box) {
    return BLGradient(BLLinearGradientValues(box.x, box.y, box.x, box.y + box.h));
}

BLImage Paint::load(const std::string &path) {
    BLImage image;

    image.read_from_file(path.c_str());

    return image;
}

void Paint::cover(BLContext &context, const BLRect &box, const BLImage &source) {
    const BLSizeI size = source.size();

    if (size.w <= 0 || size.h <= 0) {
        return;
    }

    // The larger of the two ratios fills the box; the rest is cropped.
    const double scale = std::max(box.w / size.w, box.h / size.h);
    const double wide = size.w * scale;
    const double tall = size.h * scale;

    context.save();
    context.clip_to_rect(box);
    context.blit_image(BLRect{box.x + ((box.w - wide) * 0.5), box.y + ((box.h - tall) * 0.5), wide,
                              tall},
                       source, BLRectI{0, 0, size.w, size.h});
    context.restore();
}
